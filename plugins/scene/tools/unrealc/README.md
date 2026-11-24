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
Generated mesh glTF/buffers and extracted images also live directly in `-o`.
The Unreal export directory is read-only input; neither output directory may be
inside it. Image URIs are resolved relative to the glTF. If an image was moved,
its filename is also checked beside the glTF without rewriting the export.
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
- `--exclude <node>` excludes that named node and its descendants; repeat it for more nodes.
- `--material-override <name>=<asset>` replaces a named exported material with an
  authored Frostfall material. Its persistent GUID and texture dependencies are
  retained, and its filename determines the runtime binary name. Optional
  `uvScale = <u> <v>` is baked into the generated mesh's UV0. Repeat for more materials.
- `--texture-override <uri>=<asset>` replaces a named exported image with an
  authored Frostfall texture. Its GUID and runtime filename are retained.
  Texture assets can set `swizzle` to four channels from `rgba01`.
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

The helloworld offline makefile imports `<scene>/source/<scene>.gltf`, writes
descriptions into `<scene>`, and compiles into `../runtime/<scene>`. Run `make`
for both scenes, or `make courtyard` / `make desert` for one. It excludes the
baked sky meshes and Desert's decorative light-shaft meshes; Frostfall supplies
the sky, and the exported light-shaft materials do not represent their Unreal effect.
Textures are capped at 1024. Override `MAX_TEXTURE_SIZE` and `JOBS` to change that
cook. The example enables shared material bakes for repeated meshes; set
`SHARE_MATERIAL_BAKES=0` and cook into clean destinations to preserve each actor's
exported bake instead.

Desert uses `terrain.asset` to override `M_Terrain_Blend` with the SandPacked
albedo and normal maps. `sand_albedo.asset` and `sand_normal.asset` read the
original images from `source/`. The normal map uses `normalMapRG = true` to
reconstruct normal Z from RG; the packed height/AO channels are not normals.
The landscape UVs are in metres, so `uvScale = 0.25 0.25` repeats the sand every
4 metres. These authored assets survive re-imports without modifying Unreal's glTF.
`aircon_parameters.asset` uses the separately exported `AirCondition_01_02_RMA.PNG`
in place of the malformed baked `M_AirCon_01_MetallicRoughness.png`. Its `brg1`
swizzle puts occlusion in R, roughness in G, and metallic in B. The malformed
exported image stays untouched and texture decoding remains strict.

Material override names may end in `*` to select a material family. Exact names take priority. An authored material with `fallbackOnly = true` replaces only family members without an exported base-color texture; valid Unreal bakes retain their original textures and UV mapping. This is useful for unsupported Unreal material expressions. Overrides and replacement texture descriptions belong beside the generated scene assets, outside `source/`.
