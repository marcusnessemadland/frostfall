/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_EXAMPLE_HELLOWORLD_ASSET_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_ASSET_H_HEADER_GUARD

// Engine
#include <frost/file.h>

// TODO: remove?
#include <vector>

namespace
{
	/// Example filesystem loader.
	///
	struct AssetLoader
	{
		frost::AllocatorI* allocator;

		struct Entry
		{
			frost::Guid guid;
			uint32_t type = 0;
			frost::FilePath path;
			uint16_t handle = frost::kInvalidHandle;
			bool loading = false;
		};
		std::vector<Entry> entries;

		AssetLoader(frost::AllocatorI* _allocator, const frost::FilePath& _directory = frost::FilePath("."))
			: allocator(_allocator)
		{
			discover(_directory);
		}

		void discover(const frost::FilePath& _directory)
		{
			frost::DirectoryReader directory;
			if (!frost::open(&directory, _directory, frost::ErrorAssert{}))
			{
				return;
			}

			frost::FileInfo file;
			frost::Error listing;

			while (sizeof(file) == frost::read(&directory, file, &listing))
			{
				// TODO: What are we even doing here... ?
				if (frost::strCmp(file.filePath, ".") == 0 || frost::strCmp(file.filePath, "..") == 0)
				{
					continue;
				}

				Entry entry; entry.path = _directory; entry.path.join(file.filePath);

				if (file.type == frost::FileType::Dir)
				{
					discover(entry.path);
					continue;
				}

				if (file.type != frost::FileType::File || frost::strCmp(file.filePath.getExt(), ".bin") != 0)
				{
					continue;
				}

				frost::Error probe;

				// Read file
				frost::FileReader reader;
				if (!frost::open(&reader, entry.path, &probe))
				{
					frost::printf("Could not read asset: %s\n", entry.path.getCPtr());
					continue;
				}

				// Index only the identity prefix; mesh payloads are read when requested.
				uint32_t magic = 0, payloadVersion = 0;
				uint16_t version = 0, headerSize = 0;

				frost::readHE(&reader, magic, true, &probe);
				frost::readHE(&reader, version, true, &probe);
				frost::readHE(&reader, headerSize, true, &probe);
				frost::readHE(&reader, entry.type, true, &probe);
				frost::readHE(&reader, payloadVersion, true, &probe);
				frost::read(&reader, entry.guid.data, sizeof(entry.guid.data), &probe);

				frost::close(&reader);

				// Add entry
				if (probe.isOk() && magic == frost::kAssetMagic && version == frost::kAssetVersion)
				{
					for (const Entry& existing : entries)
					{
						FROST_ASSERT(existing.guid != entry.guid || existing.type != entry.type,
							"Duplicate asset identity: %s and %s", existing.path.getCPtr(), entry.path.getCPtr());
					}
					entries.push_back(entry);
				}
			}

			frost::close(&directory);
		}

		~AssetLoader()
		{
			for (const Entry& entry : entries)
			{
				if (entry.type == frost::kAssetTypeMesh) { frost::release(frost::MeshHandle{ entry.handle }); }
				else if (entry.type == frost::kAssetTypeMaterial) { frost::release(frost::MaterialHandle{ entry.handle }); }
				else if (entry.type == frost::kAssetTypeTexture) { frost::release(frost::TextureHandle{ entry.handle }); }
				else if (entry.type == frost::kAssetTypeScene) { frost::release(frost::SceneHandle{ entry.handle }); }
			}
		}

		const frost::Memory* read(const frost::FilePath& _path, frost::Error* _err)
		{
			frost::FileReader reader;
			if (!frost::open(&reader, _path, _err))
			{
				return NULL;
			}

			const int64_t size = frost::getSize(&reader);
			const auto* memory = size >= 0 && size <= INT32_MAX ? frost::allocMemory(allocator, uint32_t(size)) : NULL;

			if (NULL != memory)
			{
				frost::read(&reader, memory->data, memory->size, _err);
			}
			else
			{
				FROST_ERROR_SET(_err, frost::kErrorReaderWriterRead, "Unable to allocate asset file memory.");
			}

			frost::close(&reader);
			return memory;
		}

		uint16_t load(uint32_t _index, frost::Error* _err)
		{
			if (!_err->isOk())
			{
				return frost::kInvalidHandle;
			}

			if (_index >= entries.size() || entries[_index].loading)
			{
				FROST_ERROR_SET(_err, frost::kErrorAssetDependency, "Unresolved asset dependency."); return frost::kInvalidHandle;
			}

			Entry& entry = entries[_index];
			if (entry.handle != frost::kInvalidHandle)
			{
				return entry.handle;
			}
			entry.loading = true;

			// Read header
			frost::AssetHeader header;

			const frost::Memory* memory = read(entry.path, _err);
			if (!frost::readAssetHeader(memory, &header, _err))
			{
				frost::release(memory); entry.loading = false; return frost::kInvalidHandle;
			}

			std::vector<frost::SceneDependency> dependencies;
			std::vector<frost::MaterialHandle> materials;
			std::vector<frost::TextureHandle> textures;

			// Read dependencies
			for (uint32_t ii = 0; ii < header.dependencyCount && _err->isOk(); ++ii)
			{
				frost::AssetDependency dependency;
				if (!frost::readAssetDependency(memory, ii, &dependency, _err))
				{
					break;
				}

				uint32_t index = 0;
				while (index < entries.size() && (entries[index].guid != dependency.guid || entries[index].type != dependency.type))
				{
					++index;
				}

				const uint16_t handle = load(index, _err);
				if (dependency.type == frost::kAssetTypeMesh)
				{
					dependencies.emplace_back(frost::MeshHandle{ handle });
				}
				else if (dependency.type == frost::kAssetTypeMaterial)
				{
					dependencies.emplace_back(frost::MaterialHandle{ handle }); materials.push_back({ handle });
				}
				else if (dependency.type == frost::kAssetTypeTexture)
				{
					dependencies.emplace_back(frost::TextureHandle{ handle }); textures.push_back({ handle });
				}
			}

			// Create asset
			if (entry.type == frost::kAssetTypeScene)
			{
				entry.handle = frost::createScene(memory, dependencies.data(), uint32_t(dependencies.size()), _err).idx;
			}
			else if (entry.type == frost::kAssetTypeMesh)
			{
				entry.handle = frost::createMesh(memory, materials.data(), uint16_t(materials.size()), _err).idx;
			}
			else if (entry.type == frost::kAssetTypeMaterial)
			{
				entry.handle = frost::createMaterial(memory, textures.data(), uint16_t(textures.size()), _err).idx;
			}
			else if (entry.type == frost::kAssetTypeTexture)
			{
				entry.handle = frost::createTexture(memory, _err).idx;
			}
			else
			{
				frost::release(memory); FROST_ERROR_SET(_err, frost::kErrorAssetDependency, "Unsupported asset type.");
			}

			entry.loading = false;
			if (!_err->isOk())
			{
				frost::printf("Could not load asset: %s\n", entry.path.getCPtr());
			}

			return entry.handle;
		}

		frost::SceneHandle loadScene(frost::StringView _name)
		{
			const frost::FilePath path(_name);
			uint32_t index = 0;
			while (index < entries.size() && (entries[index].type != frost::kAssetTypeScene
				|| frost::strCmp(entries[index].path, path) != 0))
			{
				++index;
			}

			const frost::SceneHandle handle{ load(index, frost::ErrorAssert{}) };

			if (frost::isValid(handle))
			{
				frost::addRef(handle);
			}

			return handle;
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_ASSET_H_HEADER_GUARD
