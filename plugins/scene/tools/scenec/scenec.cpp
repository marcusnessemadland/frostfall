/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <scene.h>
#include <frost/commandline.h>
#include <frost/file.h>
#include <frost/scanner.h>
#include <frost/settings.h>
#include <string>
#include <vector>

using namespace frost;

static std::vector<std::string> words(StringView _text)
{
	std::vector<std::string> result;
	Scanner scanner(_text);
	while (!scanner.isDone())
	{
		scanner.accept(Scanner::Class::Space);
		const StringView word = scanner.accept(Scanner::Class::NonSpace);
		if (!word.isEmpty()) { result.emplace_back(word.getPtr(), word.getLength()); }
	}
	return result;
}

static uint32_t entityIndex(const std::vector<std::string>& _entities, StringView _name)
{
	for (uint32_t ii = 0; ii < _entities.size(); ++ii)
	{
		if (0 == strCmp(_entities[ii].c_str(), _name)) { return ii; }
	}
	return UINT32_MAX;
}

static bool writeReference(WriterI& _writer, StringView _text, uint32_t _type,
	std::vector<AssetDependency>& _dependencies, Error& _err, bool _optional)
{
	uint32_t index = UINT32_MAX;
	if (!_text.isEmpty() && 0 != strCmp(_text, "none"))
	{
		Guid guid;
		if (!fromString(&guid, _text) || !isValid(guid)) { return false; }
		for (index = 0; index < _dependencies.size(); ++index)
		{
			if (_dependencies[index].guid == guid && _dependencies[index].type == _type) { break; }
		}
		if (index == _dependencies.size()) { _dependencies.push_back({ guid, _type }); }
	}
	else if (!_optional) { return false; }
	writeLE(&_writer, index, &_err);
	return _err.isOk();
}

static bool writeComponent(WriterI& _writer, const SceneComponentSchema& _schema,
	const std::string& _prefix, const Settings& _settings, std::vector<AssetDependency>& _dependencies,
	AllocatorI& _allocator, Error& _err)
{
	MemoryBlock block(&_allocator);
	MemoryWriter writer(&block);
	void* defaults = alloc(&_allocator, _schema.size);
	_schema.defaults(defaults);
	bool valid = true;
	for (uint32_t ii = 0; ii < _schema.numFields && valid; ++ii)
	{
		const SceneField& field = _schema.fields[ii];
		const StringView text = _settings.get((_prefix + "/" + field.name).c_str());
		const auto* value = static_cast<const uint8_t*>(defaults) + field.offset;
		Scanner scanner(text);
		if (field.type == SceneField::Float)
		{
			for (uint32_t jj = 0; jj < field.count && valid; ++jj)
			{
				float number; memCopy(&number, value + jj * sizeof(float), sizeof(float));
				if (!text.isEmpty())
				{
					scanner.accept(Scanner::Class::Space);
					valid = fromString(&number, scanner.accept(Scanner::Class::NonSpace)) && isFinite(number);
				}
				uint32_t bits; memCopy(&bits, &number, sizeof(bits)); writeLE(&writer, bits, &_err);
			}
			if (!text.isEmpty()) { scanner.accept(Scanner::Class::Space); valid &= scanner.isDone(); }
		}
		else if (field.type == SceneField::Uint32)
		{
			uint32_t number; memCopy(&number, value, sizeof(number));
			valid = text.isEmpty() || fromString(&number, text);
			writeLE(&writer, number, &_err);
		}
		else if (field.type == SceneField::Bool)
		{
			bool flag = *reinterpret_cast<const bool*>(value);
			valid = text.isEmpty() || fromString(&flag, text);
			write(&writer, uint8_t(flag), &_err);
		}
		else if (field.type == SceneField::Materials)
		{
			const auto materials = words(text);
			valid = materials.size() <= UINT16_MAX;
			writeLE(&writer, uint16_t(materials.size()), &_err);
			for (const auto& material : materials)
			{
				valid &= writeReference(writer, material.c_str(), kAssetTypeMaterial, _dependencies, _err, true);
			}
		}
		else
		{
			valid = writeReference(writer, text, field.type == SceneField::Mesh ? kAssetTypeMesh : kAssetTypeTexture,
				_dependencies, _err, field.type == SceneField::Texture);
		}
	}
	free(&_allocator, defaults);
	writeLE(&_writer, _schema.id, &_err);
	writeLE(&_writer, uint32_t(writer.seek()), &_err);
	write(&_writer, block.more(), int32_t(writer.seek()), &_err);
	return valid && _err.isOk();
}

int main(int _argc, const char* _argv[])
{
	const CommandLineOption options[] = {
		{ 'f', NULL, 1, "<file>", "Scene source description." },
		{ 'o', NULL, 1, "<file>", "Compiled scene asset." },
		{ 'h', "help", 0, NULL, "Display help." },
	};
	CommandLine command(_argc, _argv, options, FROST_COUNTOF(options));
	const char* input = command.findOption('f');
	const char* output = command.findOption('o');
	if (NULL == input || NULL == output || command.hasArg('h', "help") || NULL != command.findUnknownOption())
	{
		frost::printf("Usage: scenec -f scene.asset -o scene.bin\n"); return 1;
	}
	DefaultAllocator allocator;
	Error err;
	Settings settings(&allocator);
	FileReader reader;
	if (!open(&reader, input, &err)) { frost::printf("Unable to open scene: %s\n", input); return 1; }
	settings.read(&reader, &err); close(&reader);
	Guid guid;
	bool valid = err.isOk() && fromString(&guid, settings.get("guid")) && isValid(guid);
	const auto entities = words(settings.get("entities"));
	std::vector<uint32_t> parents;
	std::vector<AssetDependency> dependencies;
	MemoryBlock block(&allocator);
	MemoryWriter writer(&block);
	writeLE(&writer, uint32_t(entities.size()), &err);
	for (uint32_t ii = 0; ii < entities.size() && valid; ++ii)
	{
		const std::string prefix = "entities/" + entities[ii];
		const StringView parent = settings.get((prefix + "/parent").c_str());
		const uint32_t parentIndex = entityIndex(entities, parent);
		parents.push_back(parentIndex);
		valid = entities[ii].size() <= UINT16_MAX && entityIndex(entities, entities[ii].c_str()) == ii
			&& (parent.isEmpty() || parentIndex != UINT32_MAX);
		writeLE(&writer, parentIndex, &err);
		writeLE(&writer, uint16_t(entities[ii].size()), &err);
		write(&writer, entities[ii].data(), int32_t(entities[ii].size()), &err);
		const auto components = words(settings.get((prefix + "/components").c_str()));
		valid &= components.size() <= UINT16_MAX;
		writeLE(&writer, uint16_t(components.size()), &err);
		for (uint32_t jj = 0; jj < components.size() && valid; ++jj)
		{
			const SceneComponentSchema* schema = getSceneComponentSchema(components[jj].c_str());
			valid = schema != NULL && entityIndex(components, components[jj].c_str()) == jj;
			if (valid) { valid = writeComponent(writer, *schema, prefix + "/" + components[jj], settings, dependencies, allocator, err); }
		}
	}
	for (uint32_t ii = 0; ii < parents.size() && valid; ++ii)
	{
		uint32_t parent = ii, depth = 0;
		while (parent != UINT32_MAX && depth++ < parents.size()) { parent = parents[parent]; }
		valid = parent == UINT32_MAX;
	}
	if (!valid || !err.isOk()) { frost::printf("Invalid scene: %s\n", input); return 1; }
	AssetHeader header;
	header.type = kAssetTypeScene; header.payloadVersion = kSceneVersion; header.guid = guid;
	header.dependencyCount = uint32_t(dependencies.size());
	header.payloadOffset = kAssetHeaderSize + header.dependencyCount * kAssetDependencySize;
	header.payloadSize = uint32_t(writer.seek());
	uint8_t bytes[kAssetHeaderSize];
	writeAssetHeader(bytes, sizeof(bytes), header, &err);
	FileWriter file;
	if (!open(&file, output, false, &err)) { frost::printf("Unable to write scene: %s\n", output); return 1; }
	write(&file, bytes, sizeof(bytes), &err);
	for (const auto& dependency : dependencies)
	{
		writeAssetDependency(bytes, sizeof(bytes), dependency, &err);
		write(&file, bytes, kAssetDependencySize, &err);
	}
	write(&file, block.more(), int32_t(header.payloadSize), &err);
	close(&file);
	if (!err.isOk()) { remove(output); return 1; }
	return 0;
}
