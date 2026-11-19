# Mesh LODs

Use `meshc --lod --config mesh.asset -o mesh.bin`, or add `lod = true` to the
import description, to generate generic mesh LODs inside one asset. Without this
option, the asset contains only the original mesh.

LOD 0 keeps the original geometry. Up to three additional levels target 50%,
25%, and 12.5% of its triangles, with a maximum relative geometric error of 1%.
Each level is simplified from the original. Primitive and material boundaries
are preserved, and levels that do not reduce the total triangle count are omitted.
The targets are not guarantees: topology and the error limit can prevent reduction.

Every level retains the vertex attributes, primitive names, and shared material
slots needed for normal rendering. Each level has its own compact GPU buffers.
The main geometry pass currently selects LOD 0. Shadows select the coarsest
available level when the existing LOD toggle is enabled, including in the nearest
cascade; meshes without reduced levels use their original geometry.

Mesh payload version 3 stores a little-endian `uint16` LOD count followed by the
`uint16` material-slot count and the shared material-slot table. Each LOD then
stores a `uint32` group count followed by the existing group records. Levels are
ordered from highest to lowest detail. Version 2 assets remain loadable as one LOD.

The former `--simplify` and `--simplify-error` options are replaced by `--lod`.
