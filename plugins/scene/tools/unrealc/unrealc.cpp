/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <scene.h>
#include <frost/commandline.h>
#include <frost/settings.h>
#include <frost/file.h>
#include <frost/hash.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <thread>
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

static std::string pathText(const fs::path& path)
{
	const auto text = path.generic_u8string();
	return { reinterpret_cast<const char*>(text.data()), text.size() };
}
static std::string safeName(const char* name)
{
	std::string result;
	for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name ? name : ""); *p && result.size() < 100; ++p)
	{
		result += ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) ? char(*p) : '_';
	}
	return result.empty() ? "Node" : result;
}
static bool save(const fs::path& path, const void* data, size_t size)
{
	std::ofstream file(path, std::ios::binary); file.write(static_cast<const char*>(data), size); return bool(file);
}
static bool save(const fs::path& path, const std::string& text) { return save(path, text.data(), text.size()); }
static std::string guidFor(const fs::path& asset)
{
	frost::Guid guid;
	frost::DefaultAllocator allocator;
	if (fs::exists(asset))
	{
		frost::Settings settings(&allocator); frost::FileReader reader; frost::Error error;
		if (frost::open(&reader, pathText(asset).c_str(), &error))
		{
			settings.read(&reader, &error); frost::close(&reader); frost::fromString(&guid, settings.get("guid"));
		}
	}
	if (!frost::isValid(guid)) { guid = frost::generateGuid(); }
	char text[37]; frost::toString(text, sizeof(text), guid); return text;
}

struct Job { fs::path compiler; std::vector<std::string> arguments; };
static bool run(const Job& job)
{
#ifdef _WIN32
	std::vector<std::wstring> storage{ job.compiler.native() };
	for (const auto& arg : job.arguments) { storage.push_back(fs::u8path(arg).native()); }
	// _spawn joins argv into a command line; quote arguments for the child CRT.
	for (auto& arg : storage)
	{
		std::wstring quoted = L"\""; size_t slashes = 0;
		for (wchar_t c : arg)
		{
			if (c == L'\\') { ++slashes; continue; }
			quoted.append(slashes * (c == L'"' ? 2 : 1) + (c == L'"'), L'\\'); slashes = 0; quoted += c;
		}
		quoted.append(slashes * 2, L'\\'); quoted += L'"'; arg = quoted;
	}
	std::vector<const wchar_t*> args;
	for (const auto& arg : storage) { args.push_back(arg.c_str()); } args.push_back(NULL);
	const intptr_t status = _wspawnv(_P_WAIT, job.compiler.c_str(), args.data());
#else
	std::vector<std::string> storage{ job.compiler.string() };
	storage.insert(storage.end(), job.arguments.begin(), job.arguments.end());
	std::vector<char*> args;
	for (auto& arg : storage) { args.push_back(arg.data()); } args.push_back(NULL);
	const pid_t child = fork();
	if (child == 0) { execv(args[0], args.data()); _exit(127); }
	int code = 0;
	const int status = child < 0 || waitpid(child, &code, 0) < 0 || !WIFEXITED(code) ? -1 : WEXITSTATUS(code);
#endif
	if (status != 0) { std::fprintf(stderr, "%s failed (%lld): %s\n", pathText(job.compiler.filename()).c_str(), (long long)status, job.arguments[1].c_str()); }
	return status == 0;
}
static bool runJobs(const std::vector<Job>& jobs, unsigned workers)
{
	std::atomic<size_t> next{ 0 }; std::atomic<bool> success{ true };
	std::vector<std::thread> threads;
	for (unsigned ii = 0; ii < workers; ++ii)
	{
		threads.emplace_back([&]() { while (success) { const size_t index = next++; if (index >= jobs.size()) { break; }
			if (!run(jobs[index])) { success = false; } } });
	}
	for (auto& thread : threads) { thread.join(); } return success;
}

struct Mapping
{
	int uv = 0; float offset[2] = {}, scale[2] = { 1, 1 }, rotation = 0;
	bool operator==(const Mapping& other) const
	{
		return uv == other.uv && offset[0] == other.offset[0] && offset[1] == other.offset[1]
			&& scale[0] == other.scale[0] && scale[1] == other.scale[1] && rotation == other.rotation;
	}
};
static Mapping mapping(const cgltf_texture_view& view)
{
	Mapping result; result.uv = view.texcoord;
	if (view.has_transform)
	{
		result.uv = view.transform.has_texcoord ? view.transform.texcoord : result.uv;
		std::memcpy(result.offset, view.transform.offset, sizeof(result.offset));
		std::memcpy(result.scale, view.transform.scale, sizeof(result.scale)); result.rotation = view.transform.rotation;
	}
	return result;
}
static std::vector<const cgltf_texture_view*> views(const cgltf_material* material)
{
	if (!material) { return {}; }
	return { &material->pbr_metallic_roughness.base_color_texture, &material->normal_texture,
		&material->pbr_metallic_roughness.metallic_roughness_texture, &material->occlusion_texture, &material->emissive_texture };
}

// Compare decoded vertex streams, independent of glTF accessor and mesh record identities.
static std::string geometryKey(const cgltf_mesh& mesh)
{
	std::string result;
	auto append = [&](const auto& value) { result.append(reinterpret_cast<const char*>(&value), sizeof(value)); };
	append(mesh.primitives_count);
	for (size_t ii = 0; ii < mesh.primitives_count; ++ii)
	{
		const cgltf_primitive& primitive = mesh.primitives[ii];
		append(primitive.type);
		std::vector<const cgltf_attribute*> attributes;
		for (size_t jj = 0; jj < primitive.attributes_count; ++jj)
		{
			const cgltf_attribute& attribute = primitive.attributes[jj];
			if (attribute.type == cgltf_attribute_type_position || attribute.type == cgltf_attribute_type_normal
				|| attribute.type == cgltf_attribute_type_tangent || attribute.type == cgltf_attribute_type_texcoord)
			{
				attributes.push_back(&attribute);
			}
		}
		std::sort(attributes.begin(), attributes.end(), [](const auto* a, const auto* b)
		{
			if (a->type != b->type) { return a->type < b->type; }
			return a->index < b->index;
		});
		append(attributes.size());
		for (const auto* attribute : attributes)
		{
			const cgltf_accessor& accessor = *attribute->data;
			append(attribute->type); append(attribute->index); append(accessor.type); append(accessor.count);
			std::vector<float> values(accessor.count * cgltf_num_components(accessor.type));
			cgltf_accessor_unpack_floats(&accessor, values.data(), values.size());
			result.append(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(float));
		}
		const size_t count = primitive.indices ? primitive.indices->count : primitive.attributes[0].data->count;
		append(count);
		for (size_t jj = 0; jj < count; ++jj)
		{
			uint32_t index = uint32_t(jj);
			if (primitive.indices) { index = uint32_t(cgltf_accessor_read_index(primitive.indices, jj)); }
			append(index);
		}
	}
	return result;
}

struct Converter
{
	cgltf_data* data = NULL;
	fs::path input, output, runtime, tools;
	unsigned workers = 4, maxTextureSize = 0;
	bool compile = true, shareMaterialBakes = false;
	std::vector<std::string> excludes;
	std::map<std::string, fs::path> materialOverrides;
	std::map<std::string, fs::path> textureOverrides;
	std::set<std::string> usedOverrides;
	std::set<std::string> usedTextureOverrides;
	std::set<fs::path> overrideJobs;
	std::set<fs::path> textureOverrideJobs;
	unsigned relocatedImages = 0;
	std::vector<cgltf_node*> nodes;
	std::vector<std::string> nodeNames;
	std::map<const cgltf_node*, size_t> nodeIndices;
	std::map<const cgltf_material*, std::string> materials;
	std::map<const cgltf_mesh*, std::string> meshes;
	std::map<std::pair<const cgltf_image*, int>, std::string> textures;
	std::map<const cgltf_material*, Mapping> mappings;
	std::map<std::string, std::string> sharedMaterials;
	std::map<std::pair<std::string, std::string>, const cgltf_material*> sharedBakes;
	std::map<std::pair<std::vector<uint8_t>, std::string>, std::string> sharedMeshes;
	std::map<const cgltf_mesh*, std::vector<std::string>> meshMaterials;
	std::map<std::string, std::vector<std::string>> meshDefaults;
	struct TextureAsset { fs::path source; std::string guid; };
	std::map<uint32_t, std::vector<TextureAsset>> sharedTextures;
	std::map<std::string, std::string> textureDescriptions, textureGuids;
	std::set<std::string> usedTextures;
	unsigned sharedBakeCount = 0;
	std::vector<Job> textureJobs, materialJobs, meshJobs;
	unsigned blended = 0, approximated = 0;
	~Converter() { cgltf_free(data); }
	fs::path assetPath(const std::string& name) const { return output / (name + ".asset"); }
	fs::path compiler(const char* name) const
	{
		std::string filename = std::string(FROST_TOOLS_PREFIX) + name;
#ifdef _WIN32
		filename += ".exe";
#endif
		return tools / filename;
	}
	Job job(const char* name, const std::string& stem) const
	{
		return { compiler(name), { std::string(name) == "scenec" || std::string(name) == "materialc" ? "-f" : "--config",
			pathText(assetPath(stem)), "-o", pathText(runtime / (stem + ".bin")) } };
	}
	void visit(cgltf_node* node)
	{
		if (node->name && std::find(excludes.begin(), excludes.end(), node->name) != excludes.end()) { return; }
		nodeIndices[node] = nodes.size(); nodes.push_back(node);
		for (size_t ii = 0; ii < node->children_count; ++ii) { visit(node->children[ii]); }
	}
	std::string texture(cgltf_texture* texture, int usage)
	{
		const cgltf_image* image = texture->image;
		const auto key = std::make_pair(image, usage);
		if (textures.count(key)) { return textures[key]; }
		const auto override = image->uri ? textureOverrides.find(image->uri) : textureOverrides.end();
		if (override != textureOverrides.end())
		{
			frost::DefaultAllocator allocator; frost::Settings settings(&allocator);
			frost::FileReader reader; frost::Error error; frost::Guid guid;
			if (!frost::open(&reader, pathText(override->second).c_str(), &error)) { return {}; }
			settings.read(&reader, &error); frost::close(&reader);
			if (!error.isOk() || !frost::fromString(&guid, settings.get("guid")) || !frost::isValid(guid))
			{
				std::fprintf(stderr, "Texture override %s requires a persistent GUID.\n", pathText(override->second).c_str()); return {};
			}
			char identity[37]; frost::toString(identity, sizeof(identity), guid);
			const std::string stem = pathText(override->second.stem());
			textures[key] = identity; textureGuids[stem] = identity;
			usedTextureOverrides.insert(override->first);
			if (textureOverrideJobs.insert(override->second).second)
			{
				Job compileTexture{ compiler("texturec"), { "--config", pathText(override->second), "-o", pathText(runtime / (stem + ".bin")) } };
				if (maxTextureSize) { compileTexture.arguments.insert(compileTexture.arguments.end(), { "--max", std::to_string(maxTextureSize) }); }
				textureJobs.push_back(compileTexture);
			}
			return identity;
		}
		const size_t index = cgltf_image_index(data, image);
		const std::string name = "tex_" + std::to_string(index) + (usage == 0 ? "_color" : usage == 1 ? "_normal" : "_linear");
		const std::string guid = guidFor(assetPath(name));
		fs::path imagePath;
		if (image->uri && std::strncmp(image->uri, "data:", 5) != 0)
		{
			std::string uri = image->uri; uri.resize(cgltf_decode_uri(uri.data())); imagePath = input.parent_path() / fs::u8path(uri);
			if (!fs::exists(imagePath) && fs::exists(input.parent_path() / imagePath.filename()))
			{
				imagePath = input.parent_path() / imagePath.filename(); ++relocatedImages;
			}
		}
		else
		{
			const bool jpeg = (image->mime_type && std::strstr(image->mime_type, "jpeg"))
				|| (image->uri && std::strncmp(image->uri, "data:image/jpeg", 15) == 0);
			imagePath = output / ("image_" + std::to_string(index) + (jpeg ? ".jpg" : ".png"));
			if (image->buffer_view)
			{
				if (!save(imagePath, cgltf_buffer_view_data(image->buffer_view), image->buffer_view->size)) { return {}; }
			}
			else
			{
				const char* base64 = image->uri ? std::strchr(image->uri, ',') : NULL;
				if (!base64) { return {}; } ++base64;
				const size_t length = std::strlen(base64), size = length * 3 / 4 - (length && base64[length - 1] == '=') - (length > 1 && base64[length - 2] == '=');
				void* bytes = NULL; cgltf_options options{};
				if (cgltf_load_buffer_base64(&options, size, base64, &bytes) != cgltf_result_success) { return {}; }
				const bool saved = save(imagePath, bytes, size); std::free(bytes); if (!saved) { return {}; }
			}
		}
		std::ifstream imageFile(imagePath, std::ios::binary);
		if (!imageFile) { return {}; }
		const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(imageFile)), std::istreambuf_iterator<char>());
		frost::HashMurmur2A hash;
		hash.begin(); hash.add(usage); hash.add(bytes.data(), int32_t(bytes.size()));
		auto& candidates = sharedTextures[hash.end()];
		for (const TextureAsset& candidate : candidates)
		{
			std::ifstream previous(candidate.source, std::ios::binary);
			const std::vector<uint8_t> contents((std::istreambuf_iterator<char>(previous)), std::istreambuf_iterator<char>());
			if (bytes == contents)
			{
				textures[key] = candidate.guid;
				return candidate.guid;
			}
		}
		candidates.push_back({ imagePath, guid });
		textures[key] = guid;
		std::ostringstream text;
		text << "guid = " << guid << "\nsource = " << pathText(fs::relative(imagePath, output))
			<< "\nformat = " << (usage == 0 ? "BC3" : usage == 1 ? "BC5" : "BC1")
			<< "\nmips = true\nquality = fastest\nlinear = " << (usage == 0 ? "false" : "true") << '\n';
		if (usage == 1) { text << "normalMap = true\n"; }
		if (maxTextureSize) { text << "maxSize = " << maxTextureSize << '\n'; }
		// Only publish and compile textures referenced by the canonical materials.
		textureDescriptions[name] = text.str();
		textureGuids[name] = guid;
		textureJobs.push_back(job("texturec", name)); return guid;
	}
	bool material(const cgltf_material* source, const cgltf_mesh& mesh, const std::string& geometry)
	{
		if (materials.count(source)) { return true; }
		auto override = source && source->name ? materialOverrides.find(source->name) : materialOverrides.end();
		if (override == materialOverrides.end() && source && source->name)
		{
			for (auto candidate = materialOverrides.begin(); candidate != materialOverrides.end(); ++candidate)
			{
				const std::string& pattern = candidate->first;
				if (pattern.ends_with('*') && std::string(source->name).starts_with(pattern.substr(0, pattern.size() - 1)))
				{
					override = candidate; break;
				}
			}
		}
		if (override != materialOverrides.end())
		{
			frost::DefaultAllocator allocator; frost::Settings settings(&allocator);
			frost::FileReader reader; frost::Error error; frost::Guid guid;
			if (!frost::open(&reader, pathText(override->second).c_str(), &error)) { return false; }
			settings.read(&reader, &error); frost::close(&reader);
			if (!error.isOk() || !frost::fromString(&guid, settings.get("guid")) || !frost::isValid(guid))
			{
				std::fprintf(stderr, "Material override %s requires a persistent GUID.\n", pathText(override->second).c_str()); return false;
			}
			usedOverrides.insert(override->first);
			bool fallbackOnly = false;
			const auto fallback = settings.get("fallbackOnly");
			if (!fallback.isEmpty() && !frost::fromString(&fallbackOnly, fallback)) { return false; }
			if (!fallbackOnly || !source->pbr_metallic_roughness.base_color_texture.texture)
			{
				Mapping selected;
				const auto scale = settings.get("uvScale");
				if (!scale.isEmpty())
				{
					std::istringstream values(std::string(scale.getPtr(), scale.getLength()));
					if (!(values >> selected.scale[0] >> selected.scale[1]) || !(values >> std::ws).eof()
						|| !std::isfinite(selected.scale[0]) || !std::isfinite(selected.scale[1]))
					{
						std::fprintf(stderr, "Material override uvScale requires two finite values.\n"); return false;
					}
				}
				char identity[37]; frost::toString(identity, sizeof(identity), guid);
				materials[source] = identity; mappings[source] = selected;
				usedOverrides.insert(override->first);
				if (overrideJobs.insert(override->second).second)
				{
					materialJobs.push_back({ compiler("materialc"), { "-f", pathText(override->second), "-o",
						pathText(runtime / (pathText(override->second.stem()) + ".bin")) } });
				}
				return true;
			}
		}
		const size_t index = source ? cgltf_material_index(data, source) : data->materials_count;
		const std::string name = "mat_" + std::to_string(index);
		const std::string guid = guidFor(assetPath(name));
		std::vector<std::string> dependencies;
		std::ostringstream text; text << std::setprecision(9) << "guid = " << guid << '\n';
		if (!source) { text << "baseColor = 1 1 1 1\nmetallic = 1\nroughness = 1\n"; }
		else
		{
			const auto& pbr = source->pbr_metallic_roughness;
			text << "baseColor = ";
			for (int ii = 0; ii < 4; ++ii) { text << (source->unlit && ii < 3 ? 0 : pbr.base_color_factor[ii]) << ' '; }
			text << "\nmetallic = " << pbr.metallic_factor << "\nroughness = " << pbr.roughness_factor
				<< "\nnormalScale = " << source->normal_texture.scale << "\nocclusionStrength = " << source->occlusion_texture.scale
				<< "\nalphaMode = " << (source->alpha_mode == cgltf_alpha_mode_opaque ? "opaque" : "mask")
				<< "\nalphaCutoff = " << source->alpha_cutoff << "\ndoubleSided = " << (source->double_sided ? "true" : "false") << "\nemissive = ";
			for (int ii = 0; ii < 3; ++ii) { text << (source->unlit ? pbr.base_color_factor[ii] : source->emissive_factor[ii]) << ' '; }
			text << "\nemissiveStrength = " << (source->has_emissive_strength ? source->emissive_strength.emissive_strength : 1) << '\n';
			blended += source->alpha_mode == cgltf_alpha_mode_blend;
			approximated += source->has_specular || source->has_ior || source->has_clearcoat || source->has_transmission;
			const auto inputs = views(source);
			const char* names[] = { "baseColor", "normal", "metallicRoughness", "occlusion", "emissive" };
			bool mapped = false; Mapping selected;
			for (size_t ii = 0; ii < inputs.size(); ++ii)
			{
				const cgltf_texture_view* view = inputs[ii];
				if (!view->texture) { continue; }
				const Mapping next = mapping(*view);
				if (mapped && !(selected == next))
				{
					std::fprintf(stderr, "Material %s needs different UV mappings per texture; bake them to a common UV set before export.\n", source->name ? source->name : name.c_str()); return false;
				}
				mapped = true; selected = next;
				const std::string textureGuid = texture(view->texture, ii == 0 || ii == 4 ? 0 : ii == 1 ? 1 : 2);
				if (textureGuid.empty()) { return false; }
				dependencies.push_back(textureGuid);
				text << '[' << names[ii] << "]\ntexture = " << textureGuid << '\n';
				const cgltf_sampler* sampler = view->texture->sampler;
				if (sampler)
				{
					const auto wrap = [](cgltf_wrap_mode mode) { return mode == cgltf_wrap_mode_clamp_to_edge ? "clamp" : mode == cgltf_wrap_mode_mirrored_repeat ? "mirror" : "repeat"; };
					text << "wrapU = " << wrap(sampler->wrap_s) << "\nwrapV = " << wrap(sampler->wrap_t) << '\n';
					if (sampler->mag_filter == cgltf_filter_type_nearest) { text << "filter = point\n"; }
				}
			}
			if (source->unlit && pbr.base_color_texture.texture)
			{
				text << "[emissive]\ntexture = " << texture(pbr.base_color_texture.texture, 0) << '\n';
			}
			mappings[source] = selected;
		}
		const std::string description = text.str().substr(text.str().find('\n') + 1);
		std::pair<std::string, std::string> bakeKey;
		bool baked = false;
		if (shareMaterialBakes && source && source->name && mesh.name)
		{
			const std::string materialName = source->name;
			const std::string suffix = std::string("_") + mesh.name;
			bool matchingGeometry = true;
			if (materialName.size() > suffix.size() && materialName.ends_with(suffix))
			{
				// A glTF material can also be referenced by a different mesh. Keep that binding intact.
				for (size_t ii = 0; ii < data->meshes_count && matchingGeometry; ++ii)
				{
					const cgltf_mesh& other = data->meshes[ii];
					if (&other == &mesh) { continue; }
					for (size_t jj = 0; jj < other.primitives_count; ++jj)
					{
						if (other.primitives[jj].material == source && geometryKey(other) != geometry)
						{
							matchingGeometry = false;
							break;
						}
					}
				}
			}
			if (matchingGeometry && materialName.size() > suffix.size() && materialName.ends_with(suffix))
			{
				std::ostringstream settings;
				settings << materialName.substr(0, materialName.size() - suffix.size()) << '\n' << mappings[source].uv << '\n';
				std::istringstream lines(description);
				std::string line;
				while (std::getline(lines, line))
				{
					if (!line.starts_with("texture = ")) { settings << line << '\n'; }
				}
				bakeKey = { geometry, settings.str() };
				baked = true;
				const auto found = sharedBakes.find(bakeKey);
				if (found != sharedBakes.end())
				{
					materials[source] = materials[found->second];
					mappings[source] = mappings[found->second];
					++sharedBakeCount;
					return true;
				}
			}
		}
		const auto found = sharedMaterials.find(description);
		if (found != sharedMaterials.end())
		{
			materials[source] = found->second;
		}
		else
		{
			if (!save(assetPath(name), text.str())) { return false; }
			materials[source] = guid;
			sharedMaterials[description] = guid;
			usedTextures.insert(dependencies.begin(), dependencies.end());
			materialJobs.push_back(job("materialc", name));
		}
		if (baked) { sharedBakes[bakeKey] = source; }
		return true;
	}

	bool mesh(const cgltf_mesh* source)
	{
		if (meshes.count(source)) { return true; }
		const std::string name = "mesh_" + std::to_string(cgltf_mesh_index(data, source));
		const std::string guid = guidFor(assetPath(name));
		std::string geometry;
		if (shareMaterialBakes) { geometry = geometryKey(*source); }
		std::vector<std::string> slots;
		std::vector<uint8_t> bytes;
		std::vector<std::string> bufferViews, accessors, primitives;
		auto attribute = [&](const std::vector<float>& values, unsigned components, bool position)
		{
			const size_t offset = bytes.size(), size = values.size() * sizeof(float), view = bufferViews.size(), accessor = accessors.size();
			bytes.resize(offset + size); std::memcpy(bytes.data() + offset, values.data(), size);
			bufferViews.push_back("{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(size) + "}");
			std::ostringstream a; a << std::setprecision(9) << "{\"bufferView\":" << view << ",\"componentType\":5126,\"count\":" << values.size() / components << ",\"type\":\"VEC" << components << "\"";
			if (position)
			{
				for (int mode = 0; mode < 2; ++mode)
				{
					a << (mode == 0 ? ",\"min\":[" : ",\"max\":[");
					for (unsigned axis = 0; axis < 3; ++axis) { float bound = values[axis];
						for (size_t v = axis; v < values.size(); v += 3) { bound = mode == 0 ? std::min(bound, values[v]) : std::max(bound, values[v]); }
						a << (axis ? "," : "") << bound; } a << ']';
				}
			}
			a << '}'; accessors.push_back(a.str()); return accessor;
		};
		std::ostringstream asset; asset << "guid = " << guid << "\nsource = " << name << ".gltf\ncoordinateSystem = lh-up+y\nscale = 1\nflipV = false\npackNormal = 1\ntangent = true\ncompress = true\nlod = true\n[materials]\n";
		for (size_t ii = 0; ii < source->primitives_count; ++ii)
		{
			const auto& primitive = source->primitives[ii];
			if (primitive.type != cgltf_primitive_type_triangles || !material(primitive.material, *source, geometry)) { return false; }
			const cgltf_accessor *positions = NULL, *normals = NULL, *tangents = NULL, *uvs = NULL;
			const Mapping uv = mappings[primitive.material];
			for (size_t jj = 0; jj < primitive.attributes_count; ++jj)
			{
				const auto& a = primitive.attributes[jj];
				if (a.type == cgltf_attribute_type_position) { positions = a.data; }
				if (a.type == cgltf_attribute_type_normal) { normals = a.data; }
				if (a.type == cgltf_attribute_type_tangent) { tangents = a.data; }
				if (a.type == cgltf_attribute_type_texcoord && a.index == uv.uv) { uvs = a.data; }
			}
			if (!positions || !normals || !uvs) { std::fprintf(stderr, "%s needs positions, normals and UV%d.\n", name.c_str(), uv.uv); return false; }
			auto unpack = [](const cgltf_accessor* accessor, unsigned components) { std::vector<float> values(accessor->count * components);
				cgltf_accessor_unpack_floats(accessor, values.data(), values.size()); return values; };
			const auto position = unpack(positions, 3), normal = unpack(normals, 3);
			auto texcoords = unpack(uvs, 2);
			// glTF and our image uploads use an upper-left UV origin. No extra V/normal-green flip.
			const float c = std::cos(uv.rotation), s = std::sin(uv.rotation);
			for (size_t v = 0; v < texcoords.size(); v += 2)
			{
				const float u = texcoords[v] * uv.scale[0], t = texcoords[v + 1] * uv.scale[1];
				texcoords[v] = uv.offset[0] + c * u - s * t; texcoords[v + 1] = uv.offset[1] + s * u + c * t;
			}
			std::ostringstream p; p << "{\"attributes\":{\"POSITION\":" << attribute(position, 3, true)
				<< ",\"NORMAL\":" << attribute(normal, 3, false) << ",\"TEXCOORD_0\":" << attribute(texcoords, 2, false);
			// Preserve an exported tangent basis, including mesh-aware material bakes.
			if (tangents) { p << ",\"TANGENT\":" << attribute(unpack(tangents, 4), 4, false); }
			p << "},\"material\":" << ii;
			std::vector<uint32_t> indices(primitive.indices ? primitive.indices->count : positions->count);
			if (primitive.indices) { if (!cgltf_accessor_unpack_indices(primitive.indices, indices.data(), sizeof(uint32_t), indices.size())) { return false; } }
			else { for (size_t jj = 0; jj < indices.size(); ++jj) { indices[jj] = uint32_t(jj); } }
			const size_t offset = bytes.size(), size = indices.size() * sizeof(uint32_t), view = bufferViews.size(), accessor = accessors.size();
			bytes.resize(offset + size); std::memcpy(bytes.data() + offset, indices.data(), size);
			bufferViews.push_back("{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(size) + "}");
			accessors.push_back("{\"bufferView\":" + std::to_string(view) + ",\"componentType\":5125,\"count\":" + std::to_string(indices.size()) + ",\"type\":\"SCALAR\"}");
			p << ",\"indices\":" << accessor << '}'; primitives.push_back(p.str());
			slots.push_back(materials[primitive.material]);
			asset << "slot_" << ii << " = " << slots.back() << '\n';
		}
		std::ostringstream gltf;
		gltf << "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"" << name << ".bin\",\"byteLength\":" << bytes.size() << "}],";
		auto array = [&](const char* key, const std::vector<std::string>& values) { gltf << '\"' << key << "\":[";
			for (size_t ii = 0; ii < values.size(); ++ii) { gltf << (ii ? "," : "") << values[ii]; } gltf << "],"; };
		array("bufferViews", bufferViews); array("accessors", accessors);
		std::vector<std::string> materialSlots;
		for (size_t ii = 0; ii < primitives.size(); ++ii) { materialSlots.push_back("{\"name\":\"slot_" + std::to_string(ii) + "\"}"); }
		array("materials", materialSlots);
		gltf << "\"meshes\":[{\"primitives\":[";
		for (size_t ii = 0; ii < primitives.size(); ++ii) { gltf << (ii ? "," : "") << primitives[ii]; }
		gltf << "]}],\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
		const std::string description = gltf.str();
		const auto key = std::make_pair(bytes, description.substr(description.find("\"bufferViews\"")));
		meshMaterials[source] = slots;
		const auto found = sharedMeshes.find(key);
		if (found != sharedMeshes.end())
		{
			meshes[source] = found->second;
			return true;
		}
		meshes[source] = guid;
		sharedMeshes[key] = guid;
		meshDefaults[guid] = slots;
		if (!save(output / (name + ".gltf"), gltf.str()) || !save(output / (name + ".bin"), bytes.data(), bytes.size())
			|| !save(assetPath(name), asset.str())) { return false; }
		Job compileMesh = job("meshc", name); compileMesh.arguments.insert(compileMesh.arguments.end(), { "--obb", "1" });
		meshJobs.push_back(compileMesh); return true;
	}

	bool transform(const cgltf_node* node, frost::TransformComponent& result)
	{
		float m[16]; cgltf_node_transform_local(node, m);
		// C M C, C = diag(-1,1,1,1), matches meshc's RH Y-up -> LH Y-up reflection.
		for (unsigned col = 0; col < 4; ++col) { for (unsigned row = 0; row < 4; ++row) { if ((row == 0) != (col == 0)) { m[col * 4 + row] = -m[col * 4 + row]; } } }
		result.position = { m[12], m[13], m[14] };
		if (!node->has_matrix)
		{
			result.scale = { node->scale[0], node->scale[1], node->scale[2] };
			result.rotation = { node->rotation[0], -node->rotation[1], -node->rotation[2], node->rotation[3] }; return true;
		}
		frost::Vec3 x{ m[0], m[1], m[2] }, y{ m[4], m[5], m[6] }, z{ m[8], m[9], m[10] };
		result.scale = { frost::length(x), frost::length(y), frost::length(z) };
		if (result.scale.x == 0 || result.scale.y == 0 || result.scale.z == 0) { return false; }
		if (frost::dot(frost::cross(x, y), z) < 0) { result.scale.x = -result.scale.x; }
		x = frost::mul(x, 1 / result.scale.x); y = frost::mul(y, 1 / result.scale.y); z = frost::mul(z, 1 / result.scale.z);
		if (std::fabs(frost::dot(x, y)) > 0.0001f || std::fabs(frost::dot(x, z)) > 0.0001f || std::fabs(frost::dot(y, z)) > 0.0001f) { return false; }
		const float trace = x.x + y.y + z.z;
		if (trace > 0) { const float s = std::sqrt(trace + 1) * 2; result.rotation = { (y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, s / 4 }; }
		else if (x.x > y.y && x.x > z.z) { const float s = std::sqrt(1 + x.x - y.y - z.z) * 2; result.rotation = { s / 4, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s }; }
		else if (y.y > z.z) { const float s = std::sqrt(1 + y.y - x.x - z.z) * 2; result.rotation = { (y.x + x.y) / s, s / 4, (z.y + y.z) / s, (z.x - x.z) / s }; }
		else { const float s = std::sqrt(1 + z.z - x.x - y.y) * 2; result.rotation = { (z.x + x.z) / s, (z.y + y.z) / s, s / 4, (x.y - y.x) / s }; }
		return true;
	}
	bool convert()
	{
		cgltf_options options{};
		if (cgltf_parse_file(&options, pathText(input).c_str(), &data) != cgltf_result_success
			|| cgltf_load_buffers(&options, data, pathText(input).c_str()) != cgltf_result_success
			|| cgltf_validate(data) != cgltf_result_success) { return false; }
		cgltf_scene* scene = data->scene ? data->scene : data->scenes_count ? &data->scenes[0] : NULL;
		if (!scene) { return false; }
		fs::create_directories(output); fs::create_directories(runtime);
		for (size_t ii = 0; ii < scene->nodes_count; ++ii) { visit(scene->nodes[ii]); }
		std::set<std::string> names;
		for (size_t ii = 0; ii < nodes.size(); ++ii)
		{
			if (nodes[ii]->skin || (nodes[ii]->mesh && nodes[ii]->mesh->weights_count))
			{
				std::fprintf(stderr, "unrealc imports static meshes only.\n"); return false;
			}
			std::string name = safeName(nodes[ii]->name);
			while (names.count(name)) { name += '_' + std::to_string(ii); } names.insert(name); nodeNames.push_back(name);
			if (nodes[ii]->mesh && !mesh(nodes[ii]->mesh)) { return false; }
		}
		const std::string name = safeName(pathText(input.stem()).c_str());
		std::ostringstream text; text << std::setprecision(9) << "guid = " << guidFor(assetPath(name)) << "\nentities =";
		for (const auto& node : nodeNames) { text << ' ' << node; } text << '\n';
		for (size_t ii = 0; ii < nodes.size(); ++ii)
		{
			const auto* node = nodes[ii]; frost::TransformComponent local;
			if (!transform(node, local)) { std::fprintf(stderr, "Node %s has a non-SRT matrix.\n", nodeNames[ii].c_str()); return false; }
			const std::string prefix = "entities/" + nodeNames[ii];
			text << '[' << prefix << "]\ncomponents = Transform" << (node->mesh ? " Mesh" : "") << '\n';
			if (node->parent && nodeIndices.count(node->parent)) { text << "parent = " << nodeNames[nodeIndices[node->parent]] << '\n'; }
			text << '[' << prefix << "/Transform]\nposition = " << local.position.x << ' ' << local.position.y << ' ' << local.position.z
				<< "\nrotation = " << local.rotation.x << ' ' << local.rotation.y << ' ' << local.rotation.z << ' ' << local.rotation.w
				<< "\nscale = " << local.scale.x << ' ' << local.scale.y << ' ' << local.scale.z << '\n';
			if (node->mesh)
			{
				const std::string& handle = meshes[node->mesh];
				text << '[' << prefix << "/Mesh]\nhandle = " << handle << '\n';
				const auto& slots = meshMaterials[node->mesh];
				if (slots != meshDefaults[handle])
				{
					text << "materials =";
					for (const auto& slot : slots) { text << ' ' << slot; }
					text << '\n';
				}
			}
		}
		if (!save(assetPath(name), text.str())) { return false; }
		for (const auto& override : materialOverrides)
		{
			if (!usedOverrides.count(override.first)) { std::fprintf(stderr, "Material override not found in export: %s.\n", override.first.c_str()); return false; }
		}
		for (const auto& override : textureOverrides)
		{
			if (!usedTextureOverrides.count(override.first)) { std::fprintf(stderr, "Texture override not found in export: %s.\n", override.first.c_str()); return false; }
		}
		textureJobs.erase(std::remove_if(textureJobs.begin(), textureJobs.end(), [&](const Job& job)
		{
			return !usedTextures.count(textureGuids[fs::u8path(job.arguments[1]).stem().string()]);
		}), textureJobs.end());
		for (const Job& job : textureJobs)
		{
			const std::string stem = fs::u8path(job.arguments[1]).stem().string();
			const auto description = textureDescriptions.find(stem);
			if (description != textureDescriptions.end() && !save(assetPath(stem), description->second)) { return false; }
		}
		std::printf("unrealc: %zu entities, %zu shared meshes, %zu materials, %zu textures (%zu mesh records).\n",
			nodes.size(), meshJobs.size(), materialJobs.size(), textureJobs.size(), meshes.size());
		if (sharedBakeCount) { std::printf("Shared material bakes: %u repeated geometry/material pairs use their first bake.\n", sharedBakeCount); }
		if (relocatedImages) { std::printf("Resolved %u moved images by filename beside the glTF.\n", relocatedImages); }
		if (blended || approximated) { std::printf("Material approximations: %u BLEND -> MASK; %u use core PBR instead of specular/IOR/layer extensions.\n", blended, approximated); }
		if (!compile) { return true; }
		return runJobs(textureJobs, workers) && runJobs(materialJobs, workers) && runJobs(meshJobs, workers) && run(job("scenec", name));
	}
};

int main(int argc, const char* argv[])
{
	const frost::CommandLineOption options[] = {
		{ 'f', NULL, 1, "<file>", "Unreal-exported glTF or GLB." },
		{ 'o', NULL, 1, "<directory>", "Generated Frostfall source assets." },
		{ 0, "runtime", 1, "<directory>", "Compiled assets (default: <output>/runtime)." },
		{ 0, "tools", 1, "<directory>", "Compiler directory (default: beside unrealc)." },
		{ 'j', "jobs", 1, "<count>", "Parallel compiler processes (default: 4)." },
		{ 0, "max-texture-size", 1, "<pixels>", "Optional texture size limit." },
		{ 0, "exclude", 1, "<node>", "Exclude a named node and its subtree (repeatable)." },
		{ 0, "material-override", 1, "<name>=<asset>", "Use an authored Frostfall material (repeatable)." },
		{ 0, "texture-override", 1, "<uri>=<asset>", "Use an authored Frostfall texture (repeatable)." },
		{ 0, "no-compile", 0, NULL, "Generate editable source assets only." },
		{ 0, "share-material-bakes", 0, NULL, "Use one Unreal material bake per repeated geometry and material family." },
		{ 'h', "help", 0, NULL, "Display help." },
	};
	frost::CommandLine command(argc, argv, options, FROST_COUNTOF(options));
	const char* input = command.findOption('f'), *output = command.findOption('o');
	if (!input || !output || command.hasArg('h', "help") || command.findUnknownOption())
	{
		std::puts("Usage: unrealc -f level.gltf -o generated [--runtime runtime] [--tools directory] [-j 4] [--exclude node] [--material-override name=asset] [--texture-override uri=asset] [--max-texture-size pixels] [--share-material-bakes] [--no-compile]"); return 1;
	}
	Converter converter; converter.input = fs::absolute(fs::u8path(input)); converter.output = fs::absolute(fs::u8path(output));
	const auto relative = converter.output.lexically_normal().lexically_relative(converter.input.parent_path().lexically_normal());
	if (!relative.empty() && *relative.begin() != "..") { std::fprintf(stderr, "Output must be outside the Unreal export directory.\n"); return 1; }
	const char* runtime = command.findOption("runtime"), *tools = command.findOption("tools"), *jobs = command.findOption('j', "jobs"), *max = command.findOption("max-texture-size");
	converter.runtime = runtime ? fs::absolute(fs::u8path(runtime)) : converter.output / "runtime";
	const auto runtimeRelative = converter.runtime.lexically_normal().lexically_relative(converter.input.parent_path().lexically_normal());
	if (!runtimeRelative.empty() && *runtimeRelative.begin() != "..") { std::fprintf(stderr, "Runtime output must be outside the Unreal export directory.\n"); return 1; }
	converter.tools = tools ? fs::absolute(fs::u8path(tools)) : fs::u8path(pathText(fs::path(frost::FilePath(frost::Dir::Executable).getCPtr()).parent_path()));
	if (jobs && (!frost::fromString(&converter.workers, jobs) || converter.workers == 0)) { return 1; }
	if (max && !frost::fromString(&converter.maxTextureSize, max)) { return 1; }
	for (int32_t ii = 0; const char* exclude = command.findOption(ii, 0, "exclude"); ++ii) { converter.excludes.emplace_back(exclude); }
	const char* overrideOptions[] = { "material-override", "texture-override" };
	for (uint32_t type = 0; type < 2; ++type)
	{
		for (int32_t ii = 0; const char* value = command.findOption(ii, 0, overrideOptions[type]); ++ii)
		{
			const std::string entry = value; const size_t separator = entry.find('=');
			if (separator == std::string::npos || separator == 0 || separator + 1 == entry.size())
			{
				std::fprintf(stderr, "Asset override requires <name>=<asset>.\n"); return 1;
			}
			auto& overrides = type == 0 ? converter.materialOverrides : converter.textureOverrides;
			overrides[entry.substr(0, separator)] = fs::absolute(fs::u8path(entry.substr(separator + 1)));
		}
	}
	converter.compile = !command.hasArg("no-compile");
	converter.shareMaterialBakes = command.hasArg("share-material-bakes");
	const bool success = converter.convert();
	if (!success) { std::fprintf(stderr, "Unable to convert %s.\n", input); }
	return success ? 0 : 1;
}
