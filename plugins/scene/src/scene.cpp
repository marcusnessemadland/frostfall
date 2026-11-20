/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <scene.h>
#include <frost/handlealloc.h>
#include <frost/readerwriter.h>
#include <frost/bufferstorage.h>
#include <cstddef>

namespace frost
{
	// Stable schema IDs and explicit fields form the serialized contract, not native layouts.
#define SCENE_FIELD(_type, _member, _kind, ...) \
	{ #_member, SceneField::_kind, uint32_t(offsetof(_type, _member)), __VA_ARGS__ }
	static const SceneField s_transform[] = {
		SCENE_FIELD(TransformComponent, position, Float, 3),
		SCENE_FIELD(TransformComponent, rotation, Float, 4),
		SCENE_FIELD(TransformComponent, scale, Float, 3),
	};
	static const SceneField s_mesh[] = {
		SCENE_FIELD(MeshComponent, handle, Mesh, 1),
		SCENE_FIELD(MeshComponent, materials, Materials, 1),
		SCENE_FIELD(MeshComponent, castShadows, Bool, 1),
	};
	static const SceneField s_camera[] = {
		SCENE_FIELD(CameraComponent, position, Float, 3),
		SCENE_FIELD(CameraComponent, rotation, Float, 4),
		SCENE_FIELD(CameraComponent, projection, Uint32, 1),
		SCENE_FIELD(CameraComponent, fov, Float, 1),
		SCENE_FIELD(CameraComponent, orthographicHeight, Float, 1),
		SCENE_FIELD(CameraComponent, nearPlane, Float, 1),
		SCENE_FIELD(CameraComponent, farPlane, Float, 1),
		SCENE_FIELD(CameraComponent, exposure, Float, 1),
	};
	static const SceneField s_light[] = {
		SCENE_FIELD(DirectionalLightComponent, direction, Float, 3),
		SCENE_FIELD(DirectionalLightComponent, color, Float, 3),
		SCENE_FIELD(DirectionalLightComponent, intensity, Float, 1),
		SCENE_FIELD(DirectionalLightComponent, isStatic, Bool, 1),
	};
	static const SceneField s_sky[] = {
		SCENE_FIELD(SkyAtmosphereComponent, time, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, hps, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, month, Uint32, 1),
		SCENE_FIELD(SkyAtmosphereComponent, latitude, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, turbidity, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, sunSize, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, sunBloom, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, sunTint, Float, 3),
		SCENE_FIELD(SkyAtmosphereComponent, sunIntensity, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, moonCycle, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, pauseMoonCycle, Bool, 1),
		SCENE_FIELD(SkyAtmosphereComponent, moonSize, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, moonIntensity, Float, 1),
		SCENE_FIELD(SkyAtmosphereComponent, moonTint, Float, 3),
		SCENE_FIELD(SkyAtmosphereComponent, moonTexture, Texture, 1),
		SCENE_FIELD(SkyAtmosphereComponent, moonAlbedo, Float, 1),
	};
	static const SceneField s_PointLight[] = {
		SCENE_FIELD(PointLightComponent, color, Float, 3),
		SCENE_FIELD(PointLightComponent, intensity, Float, 1),
		SCENE_FIELD(PointLightComponent, range, Float, 1),
		SCENE_FIELD(PointLightComponent, sourceRadius, Float, 1),
		SCENE_FIELD(PointLightComponent, castShadows, Bool, 1),
		SCENE_FIELD(PointLightComponent, isStatic, Bool, 1),
	};
	static const SceneField s_SpotLight[] = {
		SCENE_FIELD(SpotLightComponent, color, Float, 3),
		SCENE_FIELD(SpotLightComponent, intensity, Float, 1),
		SCENE_FIELD(SpotLightComponent, range, Float, 1),
		SCENE_FIELD(SpotLightComponent, sourceRadius, Float, 1),
		SCENE_FIELD(SpotLightComponent, innerAngle, Float, 1),
		SCENE_FIELD(SpotLightComponent, outerAngle, Float, 1),
		SCENE_FIELD(SpotLightComponent, castShadows, Bool, 1),
		SCENE_FIELD(SpotLightComponent, isStatic, Bool, 1),
	};
	static const SceneField s_RectLight[] = {
		SCENE_FIELD(RectLightComponent, color, Float, 3),
		SCENE_FIELD(RectLightComponent, intensity, Float, 1),
		SCENE_FIELD(RectLightComponent, range, Float, 1),
		SCENE_FIELD(RectLightComponent, sourceRadius, Float, 1),
		SCENE_FIELD(RectLightComponent, width, Float, 1),
		SCENE_FIELD(RectLightComponent, height, Float, 1),
		SCENE_FIELD(RectLightComponent, twoSided, Bool, 1),
		SCENE_FIELD(RectLightComponent, castShadows, Bool, 1),
		SCENE_FIELD(RectLightComponent, isStatic, Bool, 1),
	};
	static const SceneField s_SkyLight[] = {
		SCENE_FIELD(SkyLightComponent, color, Float, 3),
		SCENE_FIELD(SkyLightComponent, intensity, Float, 1),
		SCENE_FIELD(SkyLightComponent, skyDistanceThreshold, Float, 1),
		SCENE_FIELD(SkyLightComponent, resolution, Uint32, 1),
		SCENE_FIELD(SkyLightComponent, isStatic, Bool, 1),
	};
#undef SCENE_FIELD

	template<typename Ty> static void sceneDefaults(void* _data) { FROST_PLACEMENT_NEW(_data, Ty)(); }
	template<typename Ty> static void sceneSpawn(World& _world, Entity _entity, const void* _data)
	{
		_world.insert(_entity, *static_cast<const Ty*>(_data));
	}
#define SCENE_COMPONENT(_id, _name, _type, _fields) \
	{ _id, _name, sizeof(_type), _fields, FROST_COUNTOF(_fields), sceneDefaults<_type>, sceneSpawn<_type> }
	static const SceneComponentSchema s_components[] = {
		SCENE_COMPONENT(1, "Transform", TransformComponent, s_transform),
		SCENE_COMPONENT(2, "Mesh", MeshComponent, s_mesh),
		SCENE_COMPONENT(3, "Camera", CameraComponent, s_camera),
		SCENE_COMPONENT(4, "DirectionalLight", DirectionalLightComponent, s_light),
		SCENE_COMPONENT(5, "SkyAtmosphere", SkyAtmosphereComponent, s_sky),
		SCENE_COMPONENT(6, "PointLight", PointLightComponent, s_PointLight),
		SCENE_COMPONENT(7, "SpotLight", SpotLightComponent, s_SpotLight),
		SCENE_COMPONENT(8, "RectLight", RectLightComponent, s_RectLight),
		SCENE_COMPONENT(9, "SkyLight", SkyLightComponent, s_SkyLight),
	};
#undef SCENE_COMPONENT

	const SceneComponentSchema* getSceneComponentSchema(uint32_t _id)
	{
		for (const auto& component : s_components) { if (component.id == _id) { return &component; } }
		return NULL;
	}
	const SceneComponentSchema* getSceneComponentSchema(StringView _name)
	{
		for (const auto& component : s_components) { if (0 == strCmp(component.name, _name)) { return &component; } }
		return NULL;
	}

	struct SceneComponentData { const SceneComponentSchema* schema = NULL; void* data = NULL; };
	struct SceneEntityData
	{
		char* name = NULL;
		uint32_t parent = UINT32_MAX;
		uint32_t numComponents = 0;
		SceneComponentData* components = NULL;
	};
	struct SceneRef
	{
		Guid guid;
		uint32_t refCount = 0;
		uint32_t numEntities = 0, numDependencies = 0;
		SceneEntityData* entities = NULL;
		SceneDependency* dependencies = NULL;
	};
	struct SceneContext { AllocatorI* allocator; HandleAlloc* handles; SceneRef* refs; };
	static SceneContext* s_sceneContext = NULL;
	static SceneRef* getSceneRef(SceneHandle _handle)
	{
		return NULL != s_sceneContext && _handle.idx < s_sceneContext->handles->getMaxHandles()
			&& s_sceneContext->handles->isValid(_handle.idx) ? &s_sceneContext->refs[_handle.idx] : NULL;
	}
	static Guid dependencyGuid(const SceneDependency& _dependency)
	{
		if (_dependency.type == kAssetTypeMesh) { return getGuid(MeshHandle{ _dependency.handle }); }
		if (_dependency.type == kAssetTypeMaterial) { return getGuid(MaterialHandle{ _dependency.handle }); }
		return _dependency.type == kAssetTypeTexture ? getGuid(TextureHandle{ _dependency.handle }) : Guid{};
	}
	static void retainDependency(const SceneDependency& _dependency)
	{
		if (_dependency.type == kAssetTypeMesh) { addRef(MeshHandle{ _dependency.handle }); }
		else if (_dependency.type == kAssetTypeMaterial) { addRef(MaterialHandle{ _dependency.handle }); }
		else { addRef(TextureHandle{ _dependency.handle }); }
	}
	static void releaseDependency(const SceneDependency& _dependency)
	{
		if (_dependency.type == kAssetTypeMesh) { release(MeshHandle{ _dependency.handle }); }
		else if (_dependency.type == kAssetTypeMaterial) { release(MaterialHandle{ _dependency.handle }); }
		else { release(TextureHandle{ _dependency.handle }); }
	}
	static void destroyScene(SceneRef& _scene)
	{
		AllocatorI* allocator = s_sceneContext->allocator;
		for (uint32_t ii = 0; ii < _scene.numEntities; ++ii)
		{
			SceneEntityData& entity = _scene.entities[ii];
			for (uint32_t jj = 0; jj < entity.numComponents; ++jj)
			{
				SceneComponentData& component = entity.components[jj];
				if (NULL != component.schema && component.schema->id == 2 && NULL != component.data)
				{
					free(allocator, const_cast<MaterialHandle*>(static_cast<MeshComponent*>(component.data)->materials));
				}
				free(allocator, component.data);
			}
			free(allocator, entity.components);
			free(allocator, entity.name);
		}
		for (uint32_t ii = 0; ii < _scene.numDependencies; ++ii) { releaseDependency(_scene.dependencies[ii]); }
		free(allocator, _scene.entities);
		free(allocator, _scene.dependencies);
		_scene = {};
	}

	void addRef(SceneHandle _handle) { ++getSceneRef(_handle)->refCount; }
	void release(SceneHandle _handle)
	{
		if (!isValid(_handle)) { return; }
		SceneRef* scene = getSceneRef(_handle);
		if (0 == --scene->refCount)
		{
			destroyScene(*scene);
			s_sceneContext->handles->free(_handle.idx);
		}
	}
	Guid getGuid(SceneHandle _handle)
	{
		const SceneRef* scene = getSceneRef(_handle);
		return NULL != scene ? scene->guid : Guid{};
	}

	static bool readDependency(MemoryReader& _reader, const SceneRef& _scene, uint32_t _type,
		uint16_t& _handle, Error* _err, bool _optional = false)
	{
		uint32_t index = UINT32_MAX;
		readHE(&_reader, index, true, _err);
		if (_optional && index == UINT32_MAX) { _handle = kInvalidHandle; return _err->isOk(); }
		if (!_err->isOk() || index >= _scene.numDependencies || _scene.dependencies[index].type != _type) { return false; }
		_handle = _scene.dependencies[index].handle;
		return true;
	}
	static bool readComponent(MemoryReader& _reader, SceneRef& _scene, SceneComponentData& _component, Error* _err)
	{
		uint32_t id = 0, size = 0;
		readHE(&_reader, id, true, _err);
		readHE(&_reader, size, true, _err);
		_component.schema = getSceneComponentSchema(id);
		if (!_err->isOk() || NULL == _component.schema || size > _reader.remaining()) { return false; }
		const int64_t end = _reader.getPos() + size;
		_component.data = alloc(s_sceneContext->allocator, _component.schema->size);
		_component.schema->defaults(_component.data);
		for (uint32_t ii = 0; ii < _component.schema->numFields; ++ii)
		{
			const SceneField& field = _component.schema->fields[ii];
			// Older light payloads omit the appended flag and retain the component default.
			if (_reader.getPos() == end && 0 == strCmp(field.name, "isStatic")) { continue; }
			uint8_t* value = static_cast<uint8_t*>(_component.data) + field.offset;
			for (uint32_t jj = 0; jj < field.count; ++jj)
			{
				switch (field.type)
				{
				case SceneField::Float:
				case SceneField::Uint32:
				{
					uint32_t bits = 0; readHE(&_reader, bits, true, _err);
					memCopy(value + jj * sizeof(bits), &bits, sizeof(bits)); break;
				}
				case SceneField::Bool:
				{
					uint8_t flag = 0; read(&_reader, flag, _err); *reinterpret_cast<bool*>(value) = flag != 0; break;
				}
				case SceneField::Mesh:
					if (!readDependency(_reader, _scene, kAssetTypeMesh, reinterpret_cast<MeshHandle*>(value)->idx, _err)) { return false; }
					break;
				case SceneField::Texture:
					if (!readDependency(_reader, _scene, kAssetTypeTexture, reinterpret_cast<TextureHandle*>(value)->idx, _err, true)) { return false; }
					break;
				case SceneField::Materials:
				{
					auto& mesh = *static_cast<MeshComponent*>(_component.data);
					uint16_t count = 0; readHE(&_reader, count, true, _err);
					if (!_err->isOk() || count > _reader.remaining() / 4) { return false; }
					auto* materials = static_cast<MaterialHandle*>(alloc(s_sceneContext->allocator, count * sizeof(MaterialHandle)));
					mesh.materials = materials; mesh.numMaterials = count;
					for (uint16_t kk = 0; kk < count; ++kk)
					{
						if (!readDependency(_reader, _scene, kAssetTypeMaterial, materials[kk].idx, _err, true)) { return false; }
					}
					break;
				}
				}
			}
		}
		return _err->isOk() && _reader.getPos() == end;
	}
	static bool readScene(const Memory* _mem, const AssetHeader& _header, SceneRef& _scene, Error* _err)
	{
		MemoryReader reader(static_cast<const uint8_t*>(_mem->data) + _header.payloadOffset, _header.payloadSize);
		uint32_t count = 0;
		readHE(&reader, count, true, _err);
		if (!_err->isOk() || count > reader.remaining() / 8) { return false; }
		_scene.entities = static_cast<SceneEntityData*>(alloc(s_sceneContext->allocator, count * sizeof(SceneEntityData)));
		for (uint32_t ii = 0; ii < count; ++ii) { FROST_PLACEMENT_NEW(&_scene.entities[ii], SceneEntityData)(); }
		_scene.numEntities = count;
		for (uint32_t ii = 0; ii < count; ++ii)
		{
			SceneEntityData& entity = _scene.entities[ii];
			uint16_t length = 0, components = 0;
			readHE(&reader, entity.parent, true, _err);
			readHE(&reader, length, true, _err);
			if (!_err->isOk() || length > reader.remaining() || (entity.parent != UINT32_MAX && entity.parent >= count)) { return false; }
			entity.name = static_cast<char*>(alloc(s_sceneContext->allocator, uint32_t(length) + 1));
			read(&reader, entity.name, length, _err); entity.name[length] = '\0';
			readHE(&reader, components, true, _err);
			if (!_err->isOk() || components > reader.remaining() / 8) { return false; }
			entity.components = static_cast<SceneComponentData*>(alloc(s_sceneContext->allocator, components * sizeof(SceneComponentData)));
			for (uint16_t jj = 0; jj < components; ++jj) { FROST_PLACEMENT_NEW(&entity.components[jj], SceneComponentData)(); }
			entity.numComponents = components;
			for (uint16_t jj = 0; jj < components; ++jj)
			{
				if (!readComponent(reader, _scene, entity.components[jj], _err)) { return false; }
			}
		}
		// Parent indices are file data; reject cycles before passing them to the ECS.
		for (uint32_t ii = 0; ii < count; ++ii)
		{
			uint32_t parent = ii, depth = 0;
			while (parent != UINT32_MAX && depth++ < count) { parent = _scene.entities[parent].parent; }
			if (parent != UINT32_MAX) { return false; }
		}
		return _err->isOk() && reader.remaining() == 0;
	}

	static SceneHandle sceneCreate(const Memory* _mem, const SceneDependency* _dependencies, uint32_t _numDependencies, Error* _err)
	{
		if (!_err->isOk()) { return {}; }
		if (NULL == s_sceneContext)
		{
			FROST_ERROR_SET(_err, kErrorSceneCreate, "Scene creation requires ScenePlugin startup."); return {};
		}
		AssetHeader header;
		if (!readAssetHeader(_mem, &header, _err)) { return {}; }
		if (header.type != kAssetTypeScene || header.payloadVersion != kSceneVersion
			|| header.dependencyCount != _numDependencies || (_numDependencies != 0 && NULL == _dependencies))
		{
			FROST_ERROR_SET(_err, kErrorSceneFormat, "Unsupported scene asset or dependency count."); return {};
		}
		SceneRef scene; scene.guid = header.guid;
		scene.dependencies = static_cast<SceneDependency*>(alloc(s_sceneContext->allocator, _numDependencies * sizeof(SceneDependency)));
		for (uint32_t ii = 0; ii < _numDependencies; ++ii)
		{
			AssetDependency dependency;
			if (!readAssetDependency(_mem, ii, &dependency, _err)
				|| dependency.type != _dependencies[ii].type || dependency.guid != dependencyGuid(_dependencies[ii]))
			{
				destroyScene(scene);
				FROST_ERROR_SET(_err, kErrorSceneFormat, "Scene dependency does not match its resolved asset."); return {};
			}
			FROST_PLACEMENT_NEW(&scene.dependencies[ii], SceneDependency)(_dependencies[ii]);
			retainDependency(_dependencies[ii]); ++scene.numDependencies;
		}
		if (!readScene(_mem, header, scene, _err))
		{
			destroyScene(scene);
			FROST_ERROR_SET(_err, kErrorSceneFormat, "Invalid scene payload."); return {};
		}
		SceneHandle handle{ s_sceneContext->handles->alloc() };
		if (!isValid(handle))
		{
			destroyScene(scene);
			FROST_ERROR_SET(_err, kErrorSceneCreate, "Scene capacity exhausted."); return {};
		}
		scene.refCount = 1; s_sceneContext->refs[handle.idx] = scene;
		return handle;
	}
	SceneHandle createScene(const Memory* _mem, const SceneDependency* _dependencies, uint32_t _numDependencies, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		const SceneHandle scene = sceneCreate(_mem, _dependencies, _numDependencies, _err);
		release(_mem);
		return scene;
	}
	SceneHandle createScene(const Memory* _mem, Error* _err) { return createScene(_mem, NULL, 0, _err); }

	// Members retain their data even if individually reparented or the root is removed.
	struct SceneMember
	{
		SceneHandle scene;
		explicit SceneMember(SceneHandle _scene) : scene(_scene) { addRef(scene); }
		SceneMember(SceneMember&& _other) : scene(_other.scene) { _other.scene = {}; }
		~SceneMember() { release(scene); }
	};
	struct SceneInstance
	{
		SceneHandle scene;
		AllocatorI* allocator;
		Entity* entities;
		SceneInstance(SceneHandle _scene, AllocatorI* _allocator, Entity* _entities)
			: scene(_scene), allocator(_allocator), entities(_entities) { addRef(scene); }
		SceneInstance(SceneInstance&& _other) : scene(_other.scene), allocator(_other.allocator), entities(_other.entities)
		{
			_other.scene = {}; _other.entities = NULL;
		}
		~SceneInstance() { free(allocator, entities); release(scene); }
	};
	Entity spawnScene(World& _world, SceneHandle _handle, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		const SceneRef* scene = getSceneRef(_handle);
		if (!_err->isOk()) { return {}; }
		if (NULL == scene) { FROST_ERROR_SET(_err, kErrorSceneCreate, "Invalid scene handle."); return {}; }
		auto* entities = static_cast<Entity*>(alloc(_world.getAllocator(), sizeof(Entity) * scene->numEntities));
		const Entity root = _world.spawn(TransformComponent{}).id();
		_world.emplace<SceneInstance>(root, _handle, _world.getAllocator(), entities);
		for (uint32_t ii = 0; ii < scene->numEntities; ++ii)
		{
			entities[ii] = _world.spawn().id();
			_world.emplace<SceneMember>(entities[ii], _handle);
			const SceneEntityData& source = scene->entities[ii];
			for (uint32_t jj = 0; jj < source.numComponents; ++jj)
			{
				const SceneComponentData& component = source.components[jj];
				component.schema->spawn(_world, entities[ii], component.data);
			}
		}
		for (uint32_t ii = 0; ii < scene->numEntities; ++ii)
		{
			const uint32_t parent = scene->entities[ii].parent;
			_world.setParent(entities[ii], parent == UINT32_MAX ? root : entities[parent]);
		}
		return root;
	}
	void despawnScene(World& _world, Entity _root)
	{
		SceneInstance* source = _world.getMut<SceneInstance>(_root);
		if (NULL == source) { return; }
		// Keep ownership local: a reparented member can now be an ancestor of the root.
		SceneInstance instance(frost::move(*source));
		_world.remove<SceneInstance>(_root);
		const uint32_t count = getSceneRef(instance.scene)->numEntities;
		for (uint32_t ii = count; ii > 0; --ii) { _world.despawn(instance.entities[ii - 1]); }
		_world.despawn(_root);
	}
	Entity findSceneEntity(const World& _world, Entity _root, StringView _name)
	{
		const SceneInstance* instance = _world.get<SceneInstance>(_root);
		if (NULL == instance) { return {}; }
		const SceneRef* scene = getSceneRef(instance->scene);
		for (uint32_t ii = 0; ii < scene->numEntities; ++ii)
		{
			if (0 == strCmp(scene->entities[ii].name, _name) && _world.contains(instance->entities[ii])) { return instance->entities[ii]; }
		}
		return {};
	}

	struct SceneSystem
	{
		static void startup(World& _world, ResT<SceneDesc> _desc)
		{
			AllocatorI* allocator = _world.getAllocator();
			s_sceneContext = static_cast<SceneContext*>(alloc(allocator, sizeof(SceneContext)));
			s_sceneContext->allocator = allocator;
			void* handles = alloc(allocator, sizeof(HandleAlloc) + 2 * _desc->maxScenes * sizeof(uint16_t));
			s_sceneContext->handles = FROST_PLACEMENT_NEW(handles, HandleAlloc)(_desc->maxScenes);
			s_sceneContext->refs = static_cast<SceneRef*>(alloc(allocator, sizeof(SceneRef) * _desc->maxScenes));
			for (uint16_t ii = 0; ii < _desc->maxScenes; ++ii) { FROST_PLACEMENT_NEW(&s_sceneContext->refs[ii], SceneRef)(); }
		}
		static void shutdown(World& _world)
		{
			DynamicStorageT<Entity> roots;
			roots.setAllocator(_world.getAllocator());
			uint32_t count = 0;
			_world.query<Entity, const SceneInstance&>().each([&](Entity _entity, const SceneInstance&)
			{
				roots.setCapacity(count + 1, count); *roots.getPtr(count++) = _entity;
			});
			for (uint32_t ii = 0; ii < count; ++ii) { despawnScene(_world, *roots.getPtr(ii)); }
			count = 0;
			_world.query<Entity, const SceneMember&>().each([&](Entity _entity, const SceneMember&)
			{
				roots.setCapacity(count + 1, count); *roots.getPtr(count++) = _entity;
			});
			for (uint32_t ii = 0; ii < count; ++ii) { _world.despawn(*roots.getPtr(ii)); }
			while (s_sceneContext->handles->getNumHandles() != 0)
			{
				const uint16_t index = s_sceneContext->handles->getHandleAt(0);
				destroyScene(s_sceneContext->refs[index]); s_sceneContext->handles->free(index);
			}
			AllocatorI* allocator = s_sceneContext->allocator;
			destroyHandleAlloc(allocator, s_sceneContext->handles);
			free(allocator, s_sceneContext->refs); free(allocator, s_sceneContext); s_sceneContext = NULL;
		}
	};
	void ScenePlugin::build(App& _app)
	{
		_app.getWorld().initResource<SceneDesc>();
		_app.addSystems(EcsStage::Startup, SceneSystem::startup);
		_app.addSystems(EcsStage::PreShutdown, SceneSystem::shutdown);
	}
}
