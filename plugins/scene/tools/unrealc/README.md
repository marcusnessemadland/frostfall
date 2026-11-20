# unrealc

Convert an Unreal glTF/GLB level export into Frostfall scene, mesh, material and
texture assets. The tool generates `.asset` descriptions and per-mesh glTF files,
then invokes `texturec`, `materialc`, `meshc` and `scenec` beside the executable.

```sh
unrealc -f courtyard.gltf -o generated --runtime runtime
```

Build the `unrealc` CMake target; its compiler dependencies build with it. Generated
sources live in `-o`; compiled `.bin` assets live in `--runtime` (default:
`<output>/runtime`). Existing generated descriptions retain their GUIDs on import.
Mesh assets are shared by vertex/index content, independently of glTF record
names or material assignments. Scene material overrides retain different slot
assignments on shared geometry. Equivalent material descriptions and identical
encoded images (with the same texture usage) also share assets. UV mappings that
produce different vertex data remain separate.

Use a clean generated/runtime destination when changing import settings so that
unreferenced files from an older cook are not packaged with the new scene.

Options:

- `--tools <directory>` selects a different compiler directory.
- `-j <count>` controls parallel compiler processes (default 4).
- `--max-texture-size <pixels>` limits texture resolution; omitted preserves it.
- `--exclude <node>` excludes that named node and its descendants.
- `--no-compile` generates source assets without running the compilers.
- `--share-material-bakes` reuses the first bake for repeated geometry and an
  original Unreal material family. Per-actor material suffixes are recognized
  only when they match the source mesh name. Geometry, selected UV channel,
  material factors, texture roles, and sampler settings must match. The first
  bake supplies its images and UV transform. This deliberately removes per-actor
  baked variation; the default preserves every exported material difference.

Unreal's exporter already converts centimeters/Z-up into glTF's meters/Y-up.
`unrealc` retains meters and uses meshc's mirror-X conversion to Frostfall's
left-handed Y-up coordinates. Scene transforms use the same basis change,
including quaternions, matrix nodes, negative scales and parent hierarchies.
Meshc converts normals, tangent handedness and triangle winding. Exported tangents
are retained; absent or invalid tangents are generated, with an orthogonal frame
for collapsed UV triangles. Local matrices must be representable
as translation, rotation and scale.

The material's selected UV set and `KHR_texture_transform` are baked into UV0.
Images and UVs retain glTF's upper-left origin; no additional V or normal-green
flip is applied. Base color/emissive textures use sRGB, normals and packed
metallic/roughness/occlusion textures use linear data. A material must use a common
UV mapping across its textures, matching the renderer's single UV channel.

This imports static mesh scenes. Cameras, lights, animation, skinning and morphs
are outside this import path. Frostfall supplies the example's camera and sky.
Core metallic/roughness materials, alpha masks, double-sided rendering and sampler
wrap modes are supported. BLEND is approximated with MASK; specular, IOR and layer
extensions use core PBR. These approximations are counted in the import summary.

The helloworld offline makefile imports `source/courtyard.gltf`, excludes the baked
`SkySphereMesh`, and caps textures at 1024 for the example. Override
`MAX_TEXTURE_SIZE` and `JOBS` when running make to change that cook. The example
enables shared material bakes for repeated courtyard meshes; set
`SHARE_MATERIAL_BAKES=0` and cook into clean destinations to preserve each actor's
exported bake instead.
