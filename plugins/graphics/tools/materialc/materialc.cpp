/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <graphics.h>
#include <frost/commandline.h>
#include <frost/file.h>
#include <frost/scanner.h>
#include <frost/settings.h>
#include <frost/debug.h>

namespace frost
{
	static bool readFactors(const Settings& _settings, const char* _name, float* _values, uint32_t _count)
	{
		const StringView text = _settings.get(_name);
		if (text.isEmpty()) { return true; }
		Scanner scanner(text);
		for (uint32_t ii = 0; ii < _count; ++ii)
		{
			scanner.accept(Scanner::Class::Space);
			if (!fromString(&_values[ii], scanner.accept(Scanner::Class::NonSpace)) || !isFinite(_values[ii]))
			{
				debugPrintf("Invalid material factor: %s.\n", _name);
				return false;
			}
		}
		scanner.accept(Scanner::Class::Space);
		return scanner.isDone();
	}

	static void writeFloat(WriterI* _writer, float _value, Error* _err)
	{
		uint32_t bits;
		memCopy(&bits, &_value, sizeof(bits));
		writeLE(_writer, bits, _err);
	}
}

int main(int _argc, const char* _argv[])
{
	using namespace frost;
	const CommandLineOption options[] =
	{
		{ 'f', NULL, 1, "<file>", "Material source description." },
		{ 'o', NULL, 1, "<file>", "Cooked material asset." },
		{ 'h', "help", 0, NULL, "Display help." },
	};
	CommandLine cmd(_argc, _argv, options, FROST_COUNTOF(options));
	const char* input = cmd.findOption('f');
	const char* output = cmd.findOption('o');
	if (NULL == input || NULL == output || NULL != cmd.findUnknownOption() || cmd.hasArg('h', "help"))
	{
		debugPrintf("Usage: materialc -f <material description> -o <runtime material>\n");
		return 1;
	}
	DefaultAllocator allocator;
	Error err;
	Settings settings(&allocator);
	FileReader reader;
	if (!open(&reader, input, &err)) { debugPrintf("Unable to open %s.\n", input); return 1; }
	settings.read(&reader, &err);
	close(&reader);
	Guid guid;
	if (!err.isOk() || !fromString(&guid, settings.get("guid")) || !isValid(guid))
	{
		debugPrintf("Material requires a persistent GUID.\n");
		return 1;
	}
	MaterialDesc desc;
	if (!readFactors(settings, "baseColor", desc.baseColor, 4)
		|| !readFactors(settings, "emissive", &desc.emissive.x, 3)
		|| !readFactors(settings, "emissiveStrength", &desc.emissiveStrength, 1)
		|| !readFactors(settings, "metallic", &desc.metallic, 1)
		|| !readFactors(settings, "roughness", &desc.roughness, 1)
		|| !readFactors(settings, "normalScale", &desc.normalScale, 1)
		|| !readFactors(settings, "occlusionStrength", &desc.occlusionStrength, 1)
		|| !readFactors(settings, "alphaCutoff", &desc.alphaCutoff, 1)) { return 1; }
	for (float value : desc.baseColor) { if (value < 0.0f || value > 1.0f) { debugPrintf("Base color must be in [0, 1].\n"); return 1; } }
	if (desc.metallic < 0.0f || desc.metallic > 1.0f || desc.roughness < 0.0f || desc.roughness > 1.0f
		|| desc.occlusionStrength < 0.0f || desc.occlusionStrength > 1.0f || desc.alphaCutoff < 0.0f || desc.alphaCutoff > 1.0f
		|| desc.emissive.x < 0.0f || desc.emissive.y < 0.0f || desc.emissive.z < 0.0f
		|| desc.emissiveStrength < 0.0f || desc.normalScale < 0.0f)
	{
		debugPrintf("Material factors are out of range.\n");
		return 1;
	}
	const StringView alpha = settings.get("alphaMode");
	if (0 == strCmp(alpha, "mask")) { desc.flags |= MaterialFlags::Masked; }
	else if (!alpha.isEmpty() && 0 != strCmp(alpha, "opaque")) { debugPrintf("Unsupported alpha mode.\n"); return 1; }
	bool doubleSided = false;
	const StringView sided = settings.get("doubleSided");
	if (!sided.isEmpty() && !fromString(&doubleSided, sided)) { debugPrintf("Invalid doubleSided.\n"); return 1; }
	if (doubleSided) { desc.flags |= MaterialFlags::DoubleSided; }

	AssetDependency dependencies[MaterialTexture::Count];
	uint32_t indices[MaterialTexture::Count];
	uint32_t numDependencies = 0;
	const char* names[] = { "baseColor", "normal", "metallicRoughness", "occlusion", "emissive" };
	for (uint32_t ii = 0; ii < MaterialTexture::Count; ++ii)
	{
		indices[ii] = UINT32_MAX;
		char key[64];
		snprintf(key, sizeof(key), "%s/texture", names[ii]);
		const StringView texture = settings.get(key);
		if (!texture.isEmpty())
		{
			Guid textureGuid;
			if (!fromString(&textureGuid, texture) || !isValid(textureGuid)) { debugPrintf("Invalid texture GUID: %s.\n", names[ii]); return 1; }
			uint32_t index = 0;
			while (index < numDependencies && dependencies[index].guid != textureGuid) { ++index; }
			if (index == numDependencies) { dependencies[numDependencies++] = { textureGuid, kAssetTypeTexture }; }
			indices[ii] = index;
		}
		snprintf(key, sizeof(key), "%s/uv", names[ii]);
		const StringView uv = settings.get(key);
		if (!uv.isEmpty() && 0 != strCmp(uv, "0")) { debugPrintf("Only UV0 is supported.\n"); return 1; }
		for (uint32_t axis = 0; axis < 2; ++axis)
		{
			snprintf(key, sizeof(key), "%s/wrap%s", names[ii], 0 == axis ? "U" : "V");
			const StringView wrap = settings.get(key);
			if (0 == strCmp(wrap, "clamp")) { desc.samplers[ii] |= 0 == axis ? TextureSampler::ClampU : TextureSampler::ClampV; }
			else if (0 == strCmp(wrap, "mirror")) { desc.samplers[ii] |= 0 == axis ? TextureSampler::MirrorU : TextureSampler::MirrorV; }
			else if (!wrap.isEmpty() && 0 != strCmp(wrap, "repeat")) { debugPrintf("Unsupported wrap mode.\n"); return 1; }
		}
		snprintf(key, sizeof(key), "%s/filter", names[ii]);
		const StringView filter = settings.get(key);
		if (0 == strCmp(filter, "point")) { desc.samplers[ii] |= TextureSampler::Point; }
		else if (0 == strCmp(filter, "anisotropic")) { desc.samplers[ii] |= TextureSampler::Anisotropic; }
		else if (!filter.isEmpty() && 0 != strCmp(filter, "linear")) { debugPrintf("Unsupported filter.\n"); return 1; }
	}
	AssetHeader header;
	header.type = kAssetTypeMaterial;
	header.payloadVersion = kMaterialVersion;
	header.guid = guid;
	header.dependencyCount = numDependencies;
	header.payloadOffset = kAssetHeaderSize + numDependencies * kAssetDependencySize;
	header.payloadSize = kMaterialPayloadSize;
	uint8_t bytes[kAssetHeaderSize];
	writeAssetHeader(bytes, sizeof(bytes), header, &err);
	FileWriter writer;
	if (!open(&writer, output, false, &err)) { debugPrintf("Unable to open %s.\n", output); return 1; }
	write(&writer, bytes, sizeof(bytes), &err);
	for (uint32_t ii = 0; ii < numDependencies; ++ii)
	{
		writeAssetDependency(bytes, sizeof(bytes), dependencies[ii], &err);
		write(&writer, bytes, kAssetDependencySize, &err);
	}
	for (float value : desc.baseColor) { writeFloat(&writer, value, &err); }
	writeFloat(&writer, desc.emissive.x, &err);
	writeFloat(&writer, desc.emissive.y, &err);
	writeFloat(&writer, desc.emissive.z, &err);
	writeFloat(&writer, desc.emissiveStrength, &err);
	writeFloat(&writer, desc.metallic, &err);
	writeFloat(&writer, desc.roughness, &err);
	writeFloat(&writer, desc.normalScale, &err);
	writeFloat(&writer, desc.occlusionStrength, &err);
	writeLE(&writer, desc.flags, &err);
	writeFloat(&writer, desc.alphaCutoff, &err);
	for (uint32_t ii = 0; ii < MaterialTexture::Count; ++ii)
	{
		writeLE(&writer, indices[ii], &err);
		writeLE(&writer, desc.samplers[ii], &err);
	}
	close(&writer);
	if (!err.isOk()) { debugPrintf("Unable to write material: %s\n", err.getMessage().getCPtr()); remove(output); return 1; }
	return 0;
}
