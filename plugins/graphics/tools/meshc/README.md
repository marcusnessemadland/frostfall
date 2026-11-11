# Mesh assets

`meshc` compiles OBJ and glTF input into a Frost mesh asset:

```sh
meshc -f cube.obj -o cube.mesh --guid a79c1f66-a424-4e05-8bcd-4a2ac19d14fb
```

The hello-world mesh is compiled explicitly with its makefile, like the graphics
shaders. CMake builds the compiler and example executable. From the repository root:

```sh
cmake --build build --config Debug --target meshc
make -C examples/helloworld
build/Debug/example-helloworld
```

The example defaults to `examples/helloworld/cube.mesh` relative to the working
directory. Use `--mesh <path>` when launching from another directory. Override
`MESHC` on the make command line to select a different compiler build.

Keep the GUID when rebuilding or moving the source asset. Supply it from source
metadata or the caller's asset database. The compiler does not generate a new
identity on each build. `--compress` enables meshoptimizer compression;
`--packnormal 1` packs normals. Run `meshc --help` for the remaining import options.

The graphics plugin accepts the resulting bytes through `createMesh`. It performs
no file I/O. The first geometry pass requires three-float positions and normals
encoded as floats, half floats, or supported normalized integers. Texture and
tangent attributes are preserved for future materials. The current default
surface uses vertex normals and a solid color without textures.

`createMesh` consumes a block produced by Frost's memory API on success or failure.
It returns one owned reference. `addRef` retains a reference and `release` removes
one. Copies of `MeshHandle`, including ECS component copies, do not retain assets.
Remove entity references before releasing the final owner. Calls that manipulate
mesh resources belong on the application thread after graphics startup and before
graphics shutdown. Each creation produces a distinct resource; GUIDs do not imply
runtime deduplication.

An entity with a `MeshHandle` is rendered. Its optional `Transform` is world-space
position, unit-quaternion rotation, and scale. Without a transform, the renderer
uses identity. Multiple entities can borrow the same mesh handle. Zero scale skips
the draw because its normal transform is singular.

## Format version 1

The asset begins with the 48-byte core `AssetHeader`, encoded by
`writeAssetHeader`. The type is `MESH`, the payload version is 1, and mesh assets
currently have no asset dependencies. All offsets in the common header are
relative to the start of the asset. Mesh material names describe slots and do not
create dependencies on material assets.

The payload starts with a little-endian `uint32_t` group count. Each group has the
following records in order:

| Record | Encoding |
|---|---|
| Bounds | Sphere center/radius (4 floats), AABB min/max (6), OBB matrix (16) |
| Vertex layout | Stride and attribute count, both `uint16_t` |
| Attributes | Eight bytes each, ordered by increasing offset |
| Buffer counts | Vertex count, index count, encoding flags; three `uint32_t` values |
| Material slot name | `uint16_t` byte length followed by UTF-8 bytes without a terminator |
| Primitive count | `uint16_t` |
| Vertex data | `uint32_t` stored byte count followed by the buffer |
| Index data | `uint32_t` stored byte count followed by the buffer |
| Primitives | Records described below |

An attribute record contains attribute enum, component count, type enum, and flags
as four bytes, followed by a `uint16_t` offset and a zero `uint16_t` reserved field.
Attribute and type enums use the bgfx values bundled with mesh format version 1.
The attribute flags are normalized (bit 0) and integer interpretation (bit 1).

Group encoding flags are compressed vertices (bit 0), compressed indices (bit 1),
and 32-bit indices (bit 2). The compiler currently emits 16-bit index groups and
splits large meshes. The runtime also supports 32-bit index payloads. Compressed
buffers use meshoptimizer's vertex and triangle-index codecs.

A primitive contains a length-prefixed name, start index, index count, start
vertex, vertex count (four `uint32_t` values), and bounds in the same encoding as
group bounds. Indices refer to the full group vertex buffer. Primitive vertex
ranges and bounds describe the optimized index data, and can overlap.

All multibyte scalar fields are little endian; floats use IEEE 754 binary32.
Vertex components and uncompressed indices are little endian as well. Structs
and runtime handles are never written directly. Changes to enum encodings or
record layouts require a mesh payload version change.
