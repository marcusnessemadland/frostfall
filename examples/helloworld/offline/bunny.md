The bunny geometry and normals come from bgfx's example asset:
plugins/graphics/3rdparty/bgfx/bgfx/examples/assets/meshes/bunny.obj

Upstream: https://github.com/bkaradzic/bgfx
License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE

Spherical UVs were added for Frostfall's deferred vertex format. Mesh positions,
normals, and triangles are preserved. The existing mesh compiler generates packed
tangents and the cooked mesh_bunny.bin from mesh_bunny.asset.
