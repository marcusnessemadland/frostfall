/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_PLUGIN_SCENE_H_HEADER_GUARD
#define FROST_PLUGIN_SCENE_H_HEADER_GUARD

#include <graphics.h>

namespace frost
{
	FROST_HANDLE(SceneHandle);
	FROST_ERROR_RESULT(kErrorSceneCreate, FROST_MAKEFOURCC('s', 'c', 'n', 'c'));
	FROST_ERROR_RESULT(kErrorSceneFormat, FROST_MAKEFOURCC('s', 'c', 'n', 'f'));
	constexpr uint32_t kAssetTypeScene = FROST_MAKEFOURCC('S', 'C', 'N', 'E');
	constexpr uint32_t kSceneVersion = 1;

	/// Resolved dependencies, in the order of the scene asset's dependency table.
	struct SceneDependency
	{
		uint32_t type;
		uint16_t handle;
		SceneDependency(MeshHandle _handle) : type(kAssetTypeMesh), handle(_handle.idx) {}
		SceneDependency(MaterialHandle _handle) : type(kAssetTypeMaterial), handle(_handle.idx) {}
		SceneDependency(TextureHandle _handle) : type(kAssetTypeTexture), handle(_handle.idx) {}
	};

	/// Consumes _mem, including on failure. Retains the resolved dependencies.
	SceneHandle createScene(const Memory* _mem, const SceneDependency* _dependencies, uint32_t _numDependencies,
		Error* _err = ErrorIgnore{});
	SceneHandle createScene(const Memory* _mem, Error* _err = ErrorIgnore{});
	void addRef(SceneHandle _handle);
	void release(SceneHandle _handle);
	Guid getGuid(SceneHandle _handle);

	/// Creates an independent entity hierarchy. Its entities retain the scene asset.
	Entity spawnScene(World& _world, SceneHandle _handle, Error* _err = ErrorIgnore{});
	/// Removes the instance's entities, including members reparented outside its root.
	void despawnScene(World& _world, Entity _root);
	Entity findSceneEntity(const World& _world, Entity _root, StringView _name);

	/// Explicit authored-field metadata shared by scenec and scene instantiation.
	struct SceneField
	{
		enum Type { Float, Uint32, Bool, Mesh, Texture, Materials };
		const char* name;
		Type type;
		uint32_t offset;
		uint32_t count = 1;
	};

	struct SceneComponentSchema
	{
		uint32_t id;
		const char* name;
		uint32_t size;
		const SceneField* fields;
		uint32_t numFields;
		void (*defaults)(void*);
		void (*spawn)(World&, Entity, const void*);
	};
	const SceneComponentSchema* getSceneComponentSchema(uint32_t _id);
	const SceneComponentSchema* getSceneComponentSchema(StringView _name);

	struct SceneDesc
	{
		uint16_t maxScenes = 256;
	};

	/// Scene asset storage. Add after GraphicsPlugin; creation is available after Startup.
	struct ScenePlugin : PluginI
	{
		virtual void build(App& _app) override;
	};
}

#endif // FROST_PLUGIN_SCENE_H_HEADER_GUARD
