/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#include <algorithm>

#include <graphics.h>
#include <frost/settings.h>
#include <frost/file.h>
#include "../../3rdparty/mikktspace/mikktspace.h"

#include <bx/string.h>
#include <bgfx/bgfx.h>
#include "vertexlayout.h"

#include <tinystl/allocator.h>
#include <tinystl/string.h>
#include <tinystl/vector.h>
namespace stl = tinystl;

#include <meshoptimizer/src/meshoptimizer.h>

#define CGLTF_VALIDATE_ENABLE_ASSERTS BX_CONFIG_DEBUG
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#define FROST_MESHC_VERSION_MAJOR 1
#define FROST_MESHC_VERSION_MINOR 0

#if 0
#	define BX_TRACE(_format, ...) \
		do { \
			bx::printf(BX_FILE_LINE_LITERAL "BGFX " _format "\n", ##__VA_ARGS__); \
		} while(0)

#	define BX_WARN(_condition, _format, ...) \
		do { \
			if (!(_condition) ) \
			{ \
				BX_TRACE(BX_FILE_LINE_LITERAL "WARN " _format, ##__VA_ARGS__); \
			} \
		} while(0)

#	define BX_ASSERT(_condition, _format, ...) \
		do { \
			if (!(_condition) ) \
			{ \
				BX_TRACE(BX_FILE_LINE_LITERAL "CHECK " _format, ##__VA_ARGS__); \
				bx::debugBreak(); \
			} \
		} while(0)
#endif // 0

#include <bx/bx.h>
#include <bx/bounds.h>
#include <bx/commandline.h>
#include <bx/debug.h>
#include <bx/file.h>
#include <bx/hash.h>
#include <bx/math.h>
#include <bx/timer.h>

typedef stl::vector<bx::Vec3> Vec3Array;

struct Index3
{
	int32_t m_position;
	int32_t m_texcoord;
	int32_t m_normal;
	int32_t m_vbc; // Barycentric ID. Holds either 0, 1 or 2.
};

struct TriIndices
{
	Index3 m_index[3];
	float m_tangents[3][4] = {};
	bool m_hasTangents = false;
};

typedef stl::vector<TriIndices> TriangleArray;

struct Group
{
	uint32_t m_startTriangle;
	uint32_t m_numTriangles;
	stl::string m_name;
	stl::string m_material;
};

typedef stl::vector<Group> GroupArray;

struct Primitive
{
	uint32_t m_startVertex;
	uint32_t m_startIndex;
	uint32_t m_numVertices;
	uint32_t m_numIndices;
	stl::string m_name;
};

typedef stl::vector<Primitive> PrimitiveArray;

static uint32_t simplifyMesh(uint16_t* _indices, uint32_t _numIndices, const uint8_t* _vertices,
	uint32_t _numVertices, uint32_t _stride, PrimitiveArray& _primitives, float _ratio, float _error)
{
	uint16_t* simplified = new uint16_t[_numIndices];
	uint32_t count = 0;
	for (PrimitiveArray::iterator it = _primitives.begin(); it != _primitives.end(); ++it)
	{
		Primitive& primitive = *it;
		const uint16_t* source = _indices + primitive.m_startIndex;
		const uint32_t target = bx::max(3u, uint32_t(float(primitive.m_numIndices) * _ratio) / 3 * 3);
		uint32_t reduced = uint32_t(meshopt_simplify(simplified + count, source,
			primitive.m_numIndices, (const float*)_vertices, _numVertices, _stride, target, _error));
		if (0 == reduced)
		{
			// Retain small/disconnected primitives instead of dropping material slots.
			reduced = primitive.m_numIndices;
			bx::memCopy(simplified + count, source, reduced * sizeof(uint16_t));
		}
		primitive.m_startIndex = count;
		primitive.m_numIndices = reduced;
		count += reduced;
	}
	bx::memCopy(_indices, simplified, count * sizeof(uint16_t));
	delete[] simplified;
	return count;
}

struct Axis
{
	enum Enum
	{
		NegativeX,
		PositiveX,
		NegativeY,
		PositiveY,
		NegativeZ,
		PositiveZ,
	};
};

static bx::Vec3 s_axisVectors[6] =
{
	bx::Vec3(-1.0f, 0.0f, 0.0f),
	bx::Vec3( 1.0f, 0.0f, 0.0f),
	bx::Vec3( 0.0f,-1.0f, 0.0f),
	bx::Vec3( 0.0f, 1.0f, 0.0f),
	bx::Vec3( 0.0f, 0.0f,-1.0f),
	bx::Vec3( 0.0f, 0.0f, 1.0f),
};

struct CoordinateSystem
{
	bx::Handedness::Enum m_handedness;
	Axis::Enum           m_up;
	Axis::Enum           m_forward;
};

struct CoordinateSystemMapping
{
	const char*      m_param;
	CoordinateSystem m_coordinateSystem;
};

static const CoordinateSystemMapping s_coordinateSystemMappings[] =
{
	{ "lh-up+y", { bx::Handedness::Left,  Axis::PositiveY, Axis::PositiveZ } },
	{ "lh-up+z", { bx::Handedness::Left,  Axis::PositiveZ, Axis::PositiveY } },
	{ "rh-up+y", { bx::Handedness::Right, Axis::PositiveY, Axis::PositiveZ } },
	{ "rh-up+z", { bx::Handedness::Right, Axis::PositiveZ, Axis::PositiveY } },
};

struct Mesh
{
	Vec3Array     m_positions;
	Vec3Array     m_normals;
	Vec3Array     m_texcoords;
	TriangleArray m_triangles;
	GroupArray    m_groups;
	cgltf_material* m_gltfMaterials = NULL;

	CoordinateSystem m_coordinateSystem;
};

static uint32_t s_obbSteps = 17;

void optimizeVertexCache(uint16_t* _indices, uint32_t _numIndices, uint32_t _numVertices)
{
	uint16_t* newIndexList = new uint16_t[_numIndices];
	meshopt_optimizeVertexCache(newIndexList, _indices, _numIndices, _numVertices);
	bx::memCopy(_indices, newIndexList, _numIndices * 2);
	delete[] newIndexList;
}

uint32_t optimizeVertexFetch(
	  uint16_t* _indices
	, uint32_t _numIndices
	, uint8_t* _vertexData
	, uint32_t _numVertices
	, uint16_t _stride
	)
{
	unsigned char* newVertices = (unsigned char*)malloc(_numVertices * _stride );
	size_t vertexCount = meshopt_optimizeVertexFetch(newVertices, _indices, _numIndices, _vertexData, _numVertices, _stride);
	bx::memCopy(_vertexData, newVertices, _numVertices * _stride);
	free(newVertices);

	return uint32_t(vertexCount);
}

void writeCompressedIndices(
	  bx::WriterI* _writer
	, const uint16_t* _indices
	, uint32_t _numIndices
	, uint32_t _numVertices
	, bx::Error* _err
	)
{
	size_t maxSize = meshopt_encodeIndexBufferBound(_numIndices, _numVertices);
	unsigned char* compressedIndices = (unsigned char*)malloc(maxSize);

	size_t compressedSize = meshopt_encodeIndexBuffer(compressedIndices, maxSize, _indices, _numIndices);
	if (0 == compressedSize)
	{
		free(compressedIndices);
		BX_ERROR_SET(_err, bx::kErrorReaderWriterWrite, "Unable to compress mesh indices.");
		return;
	}

	bx::printf("Indices uncompressed: %10d, compressed: %10d, ratio: %0.2f%%\n"
		, _numIndices*2
		, (uint32_t)compressedSize
		, 100.0f - float(compressedSize ) / float(_numIndices*2)*100.0f
		);

	bx::writeLE(_writer, (uint32_t)compressedSize, _err);
	bx::write(_writer, compressedIndices, (uint32_t)compressedSize, _err);

	free(compressedIndices);
}

void writeCompressedVertices(
	  bx::WriterI* _writer
	, const uint8_t* _vertices
	, uint32_t _numVertices
	, uint16_t _stride
	, bx::Error* _err
	)
{
	size_t maxSize = meshopt_encodeVertexBufferBound(_numVertices, _stride);
	unsigned char* compressedVertices = (unsigned char*)malloc(maxSize);

	size_t compressedSize = meshopt_encodeVertexBuffer(compressedVertices, maxSize, _vertices, _numVertices, _stride);
	if (0 == compressedSize)
	{
		free(compressedVertices);
		BX_ERROR_SET(_err, bx::kErrorReaderWriterWrite, "Unable to compress mesh vertices.");
		return;
	}

	bx::printf("Vertices uncompressed: %10d, compressed: %10d, ratio: %0.2f%%\n"
		, _numVertices * _stride
		, (uint32_t)compressedSize
		, 100.0f - float(compressedSize) / float(_numVertices * _stride)*100.0f
		);

	bx::writeLE(_writer, (uint32_t)compressedSize, _err);
	bx::write(_writer, compressedVertices, (uint32_t)compressedSize, _err);

	free(compressedVertices);
}

// MikkTSpace is evaluated per triangle corner before welding, preserving UV seams.
static void generateTangents(Mesh& _mesh)
{
	SMikkTSpaceInterface callbacks = {};
	callbacks.m_getNumFaces = [](const SMikkTSpaceContext* _context) { return int(((Mesh*)_context->m_pUserData)->m_triangles.size()); };
	callbacks.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, int) { return 3; };
	callbacks.m_getPosition = [](const SMikkTSpaceContext* _context, float _out[], int _face, int _vertex)
	{
		const Mesh& mesh = *(const Mesh*)_context->m_pUserData;
		bx::store(_out, mesh.m_positions[mesh.m_triangles[_face].m_index[_vertex].m_position]);
	};
	callbacks.m_getNormal = [](const SMikkTSpaceContext* _context, float _out[], int _face, int _vertex)
	{
		const Mesh& mesh = *(const Mesh*)_context->m_pUserData;
		bx::store(_out, bx::normalize(mesh.m_normals[mesh.m_triangles[_face].m_index[_vertex].m_normal]));
	};
	callbacks.m_getTexCoord = [](const SMikkTSpaceContext* _context, float _out[], int _face, int _vertex)
	{
		const Mesh& mesh = *(const Mesh*)_context->m_pUserData;
		const bx::Vec3& uv = mesh.m_texcoords[mesh.m_triangles[_face].m_index[_vertex].m_texcoord];
		_out[0] = uv.x;
		_out[1] = uv.y;
	};
	callbacks.m_setTSpaceBasic = [](const SMikkTSpaceContext* _context, const float _tangent[], float _sign, int _face, int _vertex)
	{
		TriIndices& triangle = ((Mesh*)_context->m_pUserData)->m_triangles[_face];
		if (!triangle.m_hasTangents)
		{
			bx::memCopy(triangle.m_tangents[_vertex], _tangent, 3 * sizeof(float));
			triangle.m_tangents[_vertex][3] = _sign;
		}
	};
	SMikkTSpaceContext context = { &callbacks, &_mesh };
	if (!genTangSpaceDefault(&context))
	{
		bx::printf("Unable to generate MikkTSpace tangents.\n");
		exit(bx::kExitFailure);
	}
}

#if BX_CPU_ENDIAN_BIG
static void swapMeshVertices(void* _data, uint32_t _count, const bgfx::VertexLayout& _layout)
{
	for (uint32_t ii = 0; ii < bgfx::Attrib::Count; ++ii)
	{
		const bgfx::Attrib::Enum attr = bgfx::Attrib::Enum(ii);
		if (!_layout.has(attr) )
		{
			continue;
		}
		uint8_t num;
		bgfx::AttribType::Enum type;
		bool normalized;
		bool asInt;
		_layout.decode(attr, num, type, normalized, asInt);
		if (bgfx::AttribType::Int8 == type || bgfx::AttribType::Uint8 == type)
		{
			continue;
		}
		const uint32_t width = bgfx::AttribType::Int16 == type || bgfx::AttribType::Uint16 == type || bgfx::AttribType::Half == type ? 2 : 4;
		const uint32_t components = bgfx::AttribType::Uint10 == type ? 1 : num;
		for (uint32_t jj = 0; jj < _count; ++jj)
		{
			uint8_t* data = (uint8_t*)_data + jj * _layout.getStride() + _layout.getOffset(attr);
			for (uint32_t kk = 0; kk < components; ++kk, data += width)
			{
				if (2 == width)
				{
					uint16_t value;
					bx::memCopy(&value, data, sizeof(value) );
					value = bx::endianSwap(value);
					bx::memCopy(data, &value, sizeof(value) );
				}
				else
				{
					uint32_t value;
					bx::memCopy(&value, data, sizeof(value) );
					value = bx::endianSwap(value);
					bx::memCopy(data, &value, sizeof(value) );
				}
			}
		}
	}
}
#endif // BX_CPU_ENDIAN_BIG


static void writeMeshFloat(bx::WriterI* _writer, float _value, bx::Error* _err)
{
	uint32_t bits;
	bx::memCopy(&bits, &_value, sizeof(bits) );
	bx::writeLE(_writer, bits, _err);
}

void write(
	  bx::WriterI* _writer
	, const void* _vertices
	, uint32_t _numVertices
	, uint32_t _stride
	, bx::Error* _err
	)
{
	bx::Sphere maxSphere;
	bx::Sphere minSphere;
	bx::calcMaxBoundingSphere(maxSphere, _vertices, _numVertices, _stride);
	bx::calcMinBoundingSphere(minSphere, _vertices, _numVertices, _stride);
	const bx::Sphere& sphere = minSphere.radius < maxSphere.radius ? minSphere : maxSphere;

	bx::Aabb aabb;
	bx::toAabb(aabb, _vertices, _numVertices, _stride);
	bx::Obb obb;
	bx::calcObb(obb, _vertices, _numVertices, _stride, s_obbSteps);

	const float bounds[] =
	{
		sphere.center.x, sphere.center.y, sphere.center.z, sphere.radius,
		aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z,
	};
	for (uint32_t ii = 0; ii < BX_COUNTOF(bounds); ++ii)
	{
		writeMeshFloat(_writer, bounds[ii], _err);
	}
	for (uint32_t ii = 0; ii < BX_COUNTOF(obb.mtx); ++ii)
	{
		writeMeshFloat(_writer, obb.mtx[ii], _err);
	}
}

void write(
	  bx::WriterI* _writer
	, const uint8_t* _vertices
	, uint32_t _numVertices
	, const bgfx::VertexLayout& _layout
	, const uint16_t* _indices
	, uint32_t _numIndices
	, bool _compress
	, uint16_t _materialSlot
	, const PrimitiveArray& _primitives
	, bx::Error* _err
	)
{
	const uint16_t stride = _layout.getStride();
	if (UINT16_MAX < _primitives.size()
	|| (_compress && (0 != stride % 4 || 256 < stride) ) )
	{
		BX_ERROR_SET(_err, bx::kErrorReaderWriterWrite, "Mesh group exceeds its format limits.");
		return;
	}
	for (PrimitiveArray::const_iterator it = _primitives.begin(); it != _primitives.end(); ++it)
	{
		if (UINT16_MAX < it->m_name.size() )
		{
			BX_ERROR_SET(_err, bx::kErrorReaderWriterWrite, "Mesh primitive name is too long.");
			return;
		}
	}
	write(_writer, _vertices, _numVertices, stride, _err);

	uint16_t numAttributes = 0;
	for (uint32_t ii = 0; ii < bgfx::Attrib::Count; ++ii)
	{
		numAttributes += _layout.has(bgfx::Attrib::Enum(ii) ) ? 1 : 0;
	}
	bx::writeLE(_writer, stride, _err);
	bx::writeLE(_writer, numAttributes, _err);

	// Preserve offsets and attribute order independently of enum ordering.
	for (uint16_t offset = 0; offset < stride; ++offset)
	{
		for (uint32_t ii = 0; ii < bgfx::Attrib::Count; ++ii)
		{
			const bgfx::Attrib::Enum attr = bgfx::Attrib::Enum(ii);
			if (_layout.has(attr) && offset == _layout.getOffset(attr) )
			{
				uint8_t num;
				bgfx::AttribType::Enum type;
				bool normalized;
				bool asInt;
				_layout.decode(attr, num, type, normalized, asInt);
				bx::write(_writer, uint8_t(attr), _err);
				bx::write(_writer, num, _err);
				bx::write(_writer, uint8_t(type), _err);
				bx::write(_writer, uint8_t( (normalized ? 1 : 0) | (asInt ? 2 : 0) ), _err);
				bx::writeLE(_writer, offset, _err);
				bx::writeLE(_writer, uint16_t(0), _err);
			}
		}
	}

	bx::writeLE(_writer, _numVertices, _err);
	bx::writeLE(_writer, _numIndices, _err);
	bx::writeLE(_writer, uint32_t(_compress ? frost::MeshFormat::VertexCompressed | frost::MeshFormat::IndexCompressed : 0), _err);
	bx::writeLE(_writer, _materialSlot, _err);
	bx::writeLE(_writer, uint16_t(_primitives.size() ), _err);

	const uint8_t* vertexData = _vertices;
#if BX_CPU_ENDIAN_BIG
	uint8_t* encodedVertices = new uint8_t[_numVertices * stride];
	bx::memCopy(encodedVertices, _vertices, _numVertices * stride);
	swapMeshVertices(encodedVertices, _numVertices, _layout);
	vertexData = encodedVertices;
#endif // BX_CPU_ENDIAN_BIG

	if (_compress)
	{
		writeCompressedVertices(_writer, vertexData, _numVertices, stride, _err);
		writeCompressedIndices(_writer, _indices, _numIndices, _numVertices, _err);
	}
	else
	{
		bx::writeLE(_writer, uint32_t(_numVertices * stride), _err);
		bx::write(_writer, vertexData, _numVertices * stride, _err);
		bx::writeLE(_writer, uint32_t(_numIndices * sizeof(uint16_t) ), _err);
		for (uint32_t ii = 0; ii < _numIndices; ++ii)
		{
			bx::writeLE(_writer, _indices[ii], _err);
		}
	}

#if BX_CPU_ENDIAN_BIG
	delete [] encodedVertices;
#endif // BX_CPU_ENDIAN_BIG

	for (PrimitiveArray::const_iterator it = _primitives.begin(); it != _primitives.end(); ++it)
	{
		const Primitive& prim = *it;
		bx::writeLE(_writer, uint16_t(prim.m_name.size() ), _err);
		bx::write(_writer, prim.m_name.c_str(), uint32_t(prim.m_name.size() ), _err);
		bx::writeLE(_writer, prim.m_startIndex, _err);
		bx::writeLE(_writer, prim.m_numIndices, _err);

		// Vertex-fetch optimization can reorder vertices across primitive ranges.
		// Bounds and ranges must be calculated from the resulting index buffer.
		uint32_t firstVertex = UINT32_MAX;
		uint32_t lastVertex = 0;
		float* positions = new float[prim.m_numIndices * 3];
		for (uint32_t ii = 0; ii < prim.m_numIndices; ++ii)
		{
			const uint16_t index = _indices[prim.m_startIndex + ii];
			firstVertex = bx::min(firstVertex, uint32_t(index) );
			lastVertex = bx::max(lastVertex, uint32_t(index) );
			float position[4];
			bgfx::vertexUnpack(position, bgfx::Attrib::Position, _layout, _vertices, index);
			bx::memCopy(&positions[ii * 3], position, 3 * sizeof(float) );
		}
		bx::writeLE(_writer, firstVertex, _err);
		bx::writeLE(_writer, lastVertex - firstVertex + 1, _err);
		write(_writer, positions, prim.m_numIndices, 3 * sizeof(float), _err);
		delete [] positions;
	}
}


inline uint32_t rgbaToAbgr(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a)
{
	return (uint32_t(_r)<<0)
		 | (uint32_t(_g)<<8)
		 | (uint32_t(_b)<<16)
		 | (uint32_t(_a)<<24)
		 ;
}

struct GroupSortByMaterial
{
	bool operator()(const Group& _lhs, const Group& _rhs)
	{
		return 0 < bx::strCmp(_lhs.m_material.c_str(), _rhs.m_material.c_str() );
	}
};

void mtxCoordinateTransform(float* _result, const CoordinateSystem& _cs)
{
	bx::Vec3 up      = s_axisVectors[_cs.m_up];
	bx::Vec3 forward = s_axisVectors[_cs.m_forward];
	bx::Vec3 right   = cross(forward,up);

	if (_cs.m_handedness == bx::Handedness::Left)
	{
		right = bx::mul(right, -1.0f);
	}

	bx::mtxIdentity(_result);
	bx::store(&_result[0], right);
	bx::store(&_result[4], forward);
	bx::store(&_result[8], up);
}

float mtxDeterminant(const float* _a)
{
	const float xx = _a[ 0];
	const float xy = _a[ 1];
	const float xz = _a[ 2];
	const float xw = _a[ 3];
	const float yx = _a[ 4];
	const float yy = _a[ 5];
	const float yz = _a[ 6];
	const float yw = _a[ 7];
	const float zx = _a[ 8];
	const float zy = _a[ 9];
	const float zz = _a[10];
	const float zw = _a[11];
	const float wx = _a[12];
	const float wy = _a[13];
	const float wz = _a[14];
	const float ww = _a[15];

	float det = 0.0f;
	det += xx * (yy*(zz*ww - zw*wz) - yz*(zy*ww - zw*wy) + yw*(zy*wz - zz*wy) );
	det -= xy * (yx*(zz*ww - zw*wz) - yz*(zx*ww - zw*wx) + yw*(zx*wz - zz*wx) );
	det += xz * (yx*(zy*ww - zw*wy) - yy*(zx*ww - zw*wx) + yw*(zx*wy - zy*wx) );
	det -= xw * (yx*(zy*wz - zz*wy) - yy*(zx*wz - zz*wx) + yz*(zx*wy - zy*wx) );

	return det;
}

void parseObj(char* _data, uint32_t _size, Mesh* _mesh, bool _hasBc)
{
	// Reference(s):
	// - Wavefront .obj file
	//   https://en.wikipedia.org/wiki/Wavefront_.obj_file

	// Coordinate system is right-handed, but up/forward is not defined, but +Y Up, +Z Forward seems to be a common default
	_mesh->m_coordinateSystem.m_handedness = bx::Handedness::Right;
	_mesh->m_coordinateSystem.m_up         = Axis::PositiveY;
	_mesh->m_coordinateSystem.m_forward    = Axis::PositiveZ;

	uint32_t num = 0;

	Group group;
	group.m_startTriangle = 0;
	group.m_numTriangles = 0;

	char commandLine[2048];
	uint32_t len = sizeof(commandLine);
	int argc;
	char* argv[64];

	for (bx::StringView next(_data, _size); !next.isEmpty(); )
	{
		next = bx::tokenizeCommandLine(next, commandLine, len, argc, argv, BX_COUNTOF(argv), '\n');

		if (0 < argc)
		{
			if (0 == bx::strCmp(argv[0], "#") )
			{
				if (2 < argc
				&&  0 == bx::strCmp(argv[2], "polygons") )
				{
				}
			}
			else if (0 == bx::strCmp(argv[0], "f") )
			{
				TriIndices triangle;
				bx::memSet(&triangle, 0, sizeof(TriIndices) );

				const int numNormals   = (int)_mesh->m_normals.size();
				const int numTexcoords = (int)_mesh->m_texcoords.size();
				const int numPositions = (int)_mesh->m_positions.size();
				for (uint32_t edge = 0, numEdges = argc-1; edge < numEdges; ++edge)
				{
					Index3 index;
					index.m_texcoord = -1;
					index.m_normal = -1;
					if (_hasBc)
					{
						index.m_vbc = edge < 3 ? edge : (1+(edge+1) )&1;
					}
					else
					{
						index.m_vbc = 0;
					}

					{
						bx::StringView triplet(argv[edge + 1]);
						bx::StringView vertex(triplet);
						bx::StringView texcoord = bx::strFind(triplet, '/');

						if (!texcoord.isEmpty() )
						{
							vertex.set(vertex.getPtr(), texcoord.getPtr() );

							const bx::StringView normal = bx::strFind(bx::StringView(texcoord.getPtr() + 1, triplet.getTerm() ), '/');
							if (!normal.isEmpty() )
							{
								int32_t nn;
								bx::fromString(&nn, bx::StringView(normal.getPtr() + 1, triplet.getTerm() ) );
								index.m_normal = (nn < 0) ? nn + numNormals : nn - 1;
							}

							texcoord.set(texcoord.getPtr() + 1, normal.getPtr() );

							// Reference(s):
							// - Wavefront .obj file / Vertex normal indices without texture coordinate indices
							//   https://en.wikipedia.org/wiki/Wavefront_.obj_file#Vertex_Normal_Indices_Without_Texture_Coordinate_Indices
							if (!texcoord.isEmpty() )
							{
								int32_t tex;
								bx::fromString(&tex, texcoord);
								index.m_texcoord = (tex < 0) ? tex + numTexcoords : tex - 1;
							}
						}

						int32_t pos;
						bx::fromString(&pos, vertex);
						index.m_position = (pos < 0) ? pos + numPositions : pos - 1;
					}

					switch (edge)
					{
					case 0: case 1: case 2:
						triangle.m_index[edge] = index;
						if (2 == edge)
						{
							_mesh->m_triangles.push_back(triangle);
						}
						break;

					default:
						triangle.m_index[1] = triangle.m_index[2];
						triangle.m_index[2] = index;

						_mesh->m_triangles.push_back(triangle);
						break;
					}
				}
			}
			else if (0 == bx::strCmp(argv[0], "g") || 0 == bx::strCmp(argv[0], "o") )
			{
				group.m_numTriangles = uint32_t(_mesh->m_triangles.size() ) - group.m_startTriangle;
				if (0 < group.m_numTriangles)
				{
					_mesh->m_groups.push_back(group);
					group.m_startTriangle = uint32_t(_mesh->m_triangles.size() );
					group.m_numTriangles = 0;
				}
				group.m_name = 1 < argc ? argv[1] : "";
			}
			else if (*argv[0] == 'v')
			{
				group.m_numTriangles = (uint32_t)(_mesh->m_triangles.size() ) - group.m_startTriangle;
				if (0 < group.m_numTriangles)
				{
					_mesh->m_groups.push_back(group);
					group.m_startTriangle = (uint32_t)(_mesh->m_triangles.size() );
					group.m_numTriangles = 0;
				}

				if (0 == bx::strCmp(argv[0], "vn") )
				{
					bx::Vec3 normal(bx::InitNone);
					bx::fromString(&normal.x, argv[1]);
					bx::fromString(&normal.y, argv[2]);
					bx::fromString(&normal.z, argv[3]);

					_mesh->m_normals.push_back(normal);
				}
				else if (0 == bx::strCmp(argv[0], "vp") )
				{
					static bool once = true;
					if (once)
					{
						once = false;
						bx::printf("warning: 'parameter space vertices' are unsupported.\n");
					}
				}
				else if (0 == bx::strCmp(argv[0], "vt") )
				{
					bx::Vec3 texcoord(bx::InitNone);
					texcoord.y = 0.0f;
					texcoord.z = 0.0f;

					bx::fromString(&texcoord.x, argv[1]);

					switch (argc)
					{
					case 4:
						bx::fromString(&texcoord.z, argv[3]);
						[[fallthrough]];

					case 3:
						bx::fromString(&texcoord.y, argv[2]);
						break;

					default:
						break;
					}

					_mesh->m_texcoords.push_back(texcoord);
				}
				else
				{
					float px, py, pz, pw;
					bx::fromString(&px, argv[1]);
					bx::fromString(&py, argv[2]);
					bx::fromString(&pz, argv[3]);

					if (argc == 5 || argc == 8)
					{
						bx::fromString(&pw, argv[4]);
					}
					else
					{
						pw = 1.0f;
					}

					bx::Vec3 pos(px, py, pz);

					const float invW = bx::rcp(pw);
					pos = bx::mul(pos, invW);

					_mesh->m_positions.push_back(pos);
				}
			}
			else if (0 == bx::strCmp(argv[0], "usemtl") )
			{
				stl::string material(argv[1]);

				if (0 != bx::strCmp(material.c_str(), group.m_material.c_str() ) )
				{
					group.m_numTriangles = (uint32_t)(_mesh->m_triangles.size() ) - group.m_startTriangle;
					if (0 < group.m_numTriangles)
					{
						_mesh->m_groups.push_back(group);
						group.m_startTriangle = (uint32_t)(_mesh->m_triangles.size() );
						group.m_numTriangles = 0;
					}
				}

				group.m_material = material;
			}
		}

		++num;
	}

	group.m_numTriangles = (uint32_t)(_mesh->m_triangles.size() ) - group.m_startTriangle;

	if (0 < group.m_numTriangles)
	{
		_mesh->m_groups.push_back(group);
		group.m_startTriangle = (uint32_t)(_mesh->m_triangles.size() );
		group.m_numTriangles  = 0;
	}

	bx::printf("obj parser # %d\n", num);
}

void gltfReadFloat(const float* _accessorData, cgltf_size _accessorNumComponents, cgltf_size _index, cgltf_float* _out, cgltf_size _outElementSize)
{
	const float* input = &_accessorData[_accessorNumComponents * _index];

	for (cgltf_size ii = 0; ii < _outElementSize; ++ii)
	{
		_out[ii] = (ii < _accessorNumComponents) ? input[ii] : 0.0f;
	}
}

void processGltfNode(cgltf_node* _node, Mesh* _mesh, Group* _group, bool _hasBc)
{
	cgltf_mesh* mesh = _node->mesh;
	if (NULL != mesh)
	{
		float nodeToWorld[16];
		cgltf_node_transform_world(_node, nodeToWorld);
		float nodeToWorldNormal[16];
		bx::mtxCofactor(nodeToWorldNormal, nodeToWorld);
		const float nodeDeterminant = bx::dot(bx::cross(bx::load<bx::Vec3>(nodeToWorld),
			bx::load<bx::Vec3>(nodeToWorld + 4)), bx::load<bx::Vec3>(nodeToWorld + 8));
		if (!bx::isFinite(nodeDeterminant) || bx::abs(nodeDeterminant) < 1.0e-12f)
		{
			bx::printf("glTF mesh requires a finite nonsingular node transform.\n");
			exit(1);
		}

		for (cgltf_size primitiveIndex = 0; primitiveIndex < mesh->primitives_count; ++primitiveIndex)
		{
			cgltf_primitive* primitive = &mesh->primitives[primitiveIndex];
			if (cgltf_primitive_type_triangles != primitive->type) { bx::printf("Only glTF triangle primitives are supported.\n"); exit(1); }
			if (NULL != primitive->material)
			{
				char materialName[64];
				bx::snprintf(materialName, sizeof(materialName), "material_%u", uint32_t(primitive->material - _mesh->m_gltfMaterials));
				_group->m_material = NULL != primitive->material->name ? primitive->material->name : materialName;
			}
			else { _group->m_material = ""; }
			struct Tangent { float value[4]; };
			stl::vector<Tangent> tangents;

			cgltf_size numVertex = primitive->attributes[0].data->count;

			int32_t basePositionIndex = (int32_t)_mesh->m_positions.size();
			int32_t baseNormalIndex   = (int32_t)_mesh->m_normals.size();
			int32_t baseTexcoordIndex = (int32_t)_mesh->m_texcoords.size();

			bool hasNormal   = false;
			bool hasTexcoord = false;

			for (cgltf_size attributeIndex = 0; attributeIndex < primitive->attributes_count; ++attributeIndex)
			{
				cgltf_attribute* attribute = &primitive->attributes[attributeIndex];
				cgltf_accessor* accessor = attribute->data;
				cgltf_size accessorCount = accessor->count;

				BX_ASSERT(numVertex == accessorCount, "Invalid attribute count");

				cgltf_size floatCount = cgltf_accessor_unpack_floats(accessor, NULL, 0);
				float* accessorData = (float*)malloc(floatCount * sizeof(float) );
				cgltf_accessor_unpack_floats(accessor, accessorData, floatCount);

				cgltf_size numComponents = cgltf_num_components(accessor->type);

				if (attribute->type == cgltf_attribute_type_position && attribute->index == 0)
				{
					_mesh->m_positions.reserve(_mesh->m_positions.size() + accessorCount);

					bx::Vec3 pos(bx::InitNone);

					for (cgltf_size v = 0; v < accessorCount; ++v)
					{
						gltfReadFloat(accessorData, numComponents, v, &pos.x, 3);
						pos = mul(pos, nodeToWorld);
						_mesh->m_positions.push_back(pos);
					}
				}
				else if (attribute->type == cgltf_attribute_type_normal && attribute->index == 0)
				{
					_mesh->m_normals.reserve(_mesh->m_normals.size() + accessorCount);

					hasNormal = true;
					bx::Vec3 normal(bx::InitNone);

					for (cgltf_size v = 0; v < accessorCount; ++v)
					{
						gltfReadFloat(accessorData, numComponents, v, &normal.x, 3);
						normal = bx::mul(bx::mulXyz0(normal, nodeToWorldNormal), nodeDeterminant < 0.0f ? -1.0f : 1.0f);
						_mesh->m_normals.push_back(normal);
					}
				}
				else if (attribute->type == cgltf_attribute_type_tangent && attribute->index == 0)
				{
					tangents.resize(accessorCount);
					for (cgltf_size v = 0; v < accessorCount; ++v)
					{
						gltfReadFloat(accessorData, numComponents, v, tangents[v].value, 4);
						const bx::Vec3 tangent = bx::mulXyz0(bx::load<bx::Vec3>(tangents[v].value), nodeToWorld);
						bx::store(tangents[v].value, bx::normalize(tangent));
						tangents[v].value[3] *= nodeDeterminant < 0.0f ? -1.0f : 1.0f;
					}
				}
				else if (attribute->type == cgltf_attribute_type_texcoord && attribute->index == 0)
				{
					_mesh->m_texcoords.reserve(_mesh->m_texcoords.size() + accessorCount);

					hasTexcoord = true;
					bx::Vec3 texcoord(bx::InitNone);

					for (cgltf_size v = 0; v < accessorCount; ++v)
					{
						gltfReadFloat(accessorData, numComponents, v, &texcoord.x, 3);
						_mesh->m_texcoords.push_back(texcoord);
					}
				}

				free(accessorData);
			}

			if (primitive->indices != NULL)
			{
				cgltf_accessor* accessor = primitive->indices;

				for (cgltf_size v = 0; v < accessor->count; v += 3)
				{
					TriIndices triangle;
					for (int i = 0; i < 3; ++i)
					{
						Index3 index;
						int32_t vertexIndex = int32_t(cgltf_accessor_read_index(accessor, v+i) );
						index.m_position = basePositionIndex + vertexIndex;
						index.m_normal   = hasNormal   ? baseNormalIndex   + vertexIndex : -1;
						index.m_texcoord = hasTexcoord ? baseTexcoordIndex + vertexIndex : -1;
						index.m_vbc      = _hasBc      ? i                               :  0;
						triangle.m_index[i] = index;
						if (!tangents.empty())
						{
							triangle.m_hasTangents = true;
							bx::memCopy(triangle.m_tangents[i], tangents[vertexIndex].value, 4 * sizeof(float));
						}
					}
					if (nodeDeterminant < 0.0f)
					{
						bx::swap(triangle.m_index[1], triangle.m_index[2]);
						for (uint32_t jj = 0; jj < 4; ++jj) { bx::swap(triangle.m_tangents[1][jj], triangle.m_tangents[2][jj]); }
					}
					_mesh->m_triangles.push_back(triangle);
				}
			}
			else
			{
				for (cgltf_size v = 0; v < numVertex; v += 3)
				{
					TriIndices triangle;
					for (int i = 0; i < 3; ++i)
					{
						Index3 index;
						int32_t vertexIndex = int32_t(v + i);
						index.m_position = basePositionIndex + vertexIndex;
						index.m_normal   = hasNormal   ? baseNormalIndex   + vertexIndex : -1;
						index.m_texcoord = hasTexcoord ? baseTexcoordIndex + vertexIndex : -1;
						index.m_vbc      = _hasBc      ? i                               :  0;
						triangle.m_index[i] = index;
						if (!tangents.empty())
						{
							triangle.m_hasTangents = true;
							bx::memCopy(triangle.m_tangents[i], tangents[vertexIndex].value, 4 * sizeof(float));
						}
					}
					if (nodeDeterminant < 0.0f)
					{
						bx::swap(triangle.m_index[1], triangle.m_index[2]);
						for (uint32_t jj = 0; jj < 4; ++jj) { bx::swap(triangle.m_tangents[1][jj], triangle.m_tangents[2][jj]); }
					}
					_mesh->m_triangles.push_back(triangle);
				}
			}

			_group->m_numTriangles = (uint32_t)(_mesh->m_triangles.size() ) - _group->m_startTriangle;

			if (0 < _group->m_numTriangles)
			{
				_mesh->m_groups.push_back(*_group);
				_group->m_startTriangle = (uint32_t)(_mesh->m_triangles.size() );
				_group->m_numTriangles = 0;
			}
		}
	}

	for (cgltf_size childIndex = 0; childIndex < _node->children_count; ++childIndex)
	{
		processGltfNode(_node->children[childIndex], _mesh, _group, _hasBc);
	}
}

void parseGltf(char* _data, uint32_t _size, Mesh* _mesh, bool _hasBc, const bx::StringView& _path)
{
	// Reference(s):
	// - Gltf 2.0 specification
	//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0

	_mesh->m_coordinateSystem.m_handedness = bx::Handedness::Right;
	_mesh->m_coordinateSystem.m_forward  = Axis::PositiveZ;
	_mesh->m_coordinateSystem.m_up       = Axis::PositiveY;

	Group group;
	group.m_startTriangle = 0;
	group.m_numTriangles = 0;

	cgltf_options options = { };
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse(&options, _data, _size, &data);

	if (result == cgltf_result_success)
	{
		char* path = (char*)malloc(_path.getLength()+1);
		bx::memCopy(path, _path.getPtr(), _path.getLength() );
		path[_path.getLength()] = 0;
		result = cgltf_load_buffers(&options, data, path);
		free(path);

		if (result == cgltf_result_success)
		{
			_mesh->m_gltfMaterials = data->materials;
			for (cgltf_size sceneIndex = 0; sceneIndex < data->scenes_count; ++sceneIndex)
			{
				cgltf_scene* scene = &data->scenes[sceneIndex];

				for (cgltf_size nodeIndex = 0; nodeIndex < scene->nodes_count; ++nodeIndex)
				{
					cgltf_node* node = scene->nodes[nodeIndex];

					processGltfNode(node, _mesh, &group, _hasBc);
				}
			}
		}

		cgltf_free(data);
	}
}

static const bx::CommandLineOption s_options[] =
{
	{ '\0', "simplify", 1, "<ratio>", "Triangle target ratio, (0, 1]. Default: 1." },
	{ '\0', "simplify-error", 1, "<error>", "Maximum relative simplification error, [0, 1]. Default: 0.01." },
	{ '\0', "config",      1, "<file>", "Frost mesh import description." },
	{ '\0', "guid",        1, "<guid>",      "Persistent asset GUID (required). Reuse it when rebuilding." },
	{ 'h',  "help",        0, NULL,          "Display this help and exit."                                 },
	{ 'v',  "version",     0, NULL,          "Output version information and exit."                        },
	{ 'f',  NULL,          1, "<file path>", "Input's file path.\n"
	                                         "Input and output may also be passed positionally, without\n"
	                                         "-f/-o."                                                      },
	{ 'o',  NULL,          1, "<file path>", "Output's file path."                                         },
	{ 's',  "scale",       1, "<num>",       "Scale factor."                                               },
	{ '\0', "ccw",         0, NULL,          "Front face is counter-clockwise winding order."              },
	{ '\0', "flipv",       0, NULL,          "Flip texture coordinate V."                                  },
	{ '\0', "obb",         1, "<num>",       "Number of steps for calculating oriented bounding box.\n"
	                                         "Defaults to 17.\n"
	                                         "Less steps = less precise OBB.\n"
	                                         "More steps = slower calculation."                            },
	{ '\0', "packnormal",  1, "<num>",       "Normal packing.\n"
	                                         "0 - unpacked 12 bytes. (default)\n"
	                                         "1 - packed 4 bytes."                                         },
	{ '\0', "packuv",      1, "<num>",       "Texture coordinate packing.\n"
	                                         "0 - unpacked 8 bytes. (default)\n"
	                                         "1 - packed 4 bytes."                                         },
	{ '\0', "tangent",     0, NULL,          "Calculate tangent vectors. (packing mode is the same as normal)" },
	{ '\0', "barycentric", 0, NULL,          "Adds barycentric vertex attribute. (Packed in bgfx::Attrib::Color1)" },
	{ 'c',  "compress",    0, NULL,          "Compress indices."                                           },
	{ '\0', "lh-up+y",     0, NULL,          "Coordinate system, Left-Handed +Y is up. (default)"          },
	{ '\0', "lh-up+z",     0, NULL,          "Coordinate system, Left-Handed +Z is up."                    },
	{ '\0', "rh-up+y",     0, NULL,          "Coordinate system, Right-Handed +Y is up."                   },
	{ '\0', "rh-up+z",     0, NULL,          "Coordinate system, Right-Handed +Z is up."                   },
};

void help(const char* _error = NULL)
{
	if (NULL != _error)
	{
		bx::printf("Error:\n%s\n\n", _error);
	}

	bx::printf(
		  "meshc, frost mesh compiler tool, version %d.%d.%d.\n"
		  "Copyright 2011-2026 Branimir Karadzic. All rights reserved.\n"
		  "License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE\n\n"
		, FROST_MESHC_VERSION_MAJOR
		, FROST_MESHC_VERSION_MINOR
		, BGFX_API_VERSION
		);

	bx::printf(
		  "Usage: meshc -f <in> -o <out>\n"
		  "       meshc <in> <out>\n"

		  "\n"
		  "Supported input file types:\n"
		  "    *.obj                  Wavefront\n"
		  "    *.gltf,*.glb           glTF 2.0\n"

		  "\n"
		  "Options:\n"
		);

	bx::Error err;
	bx::write(bx::getStdOut(), s_options, BX_COUNTOF(s_options), &err);

	bx::printf(
		  "\n"
		  "For additional information, see https://github.com/bkaradzic/bgfx\n"
		);
}

int main(int _argc, const char* _argv[])
{
	bx::CommandLine cmdLine(_argc, _argv, s_options, BX_COUNTOF(s_options) );

	if (cmdLine.hasArg('v', "version") )
	{
		bx::printf(
			"meshc, frost mesh compiler tool, version %d.%d.%d.\n"
			, FROST_MESHC_VERSION_MAJOR
			, FROST_MESHC_VERSION_MINOR
			, BGFX_API_VERSION
		);
		return bx::kExitSuccess;
	}

	if (cmdLine.hasArg('h', "help") )
	{
		help();
		return bx::kExitFailure;
	}

	const char* unknown = cmdLine.findUnknownOption();
	if (NULL != unknown)
	{
		char error[256];
		bx::snprintf(error, BX_COUNTOF(error), "Unknown option '%s'.", unknown);
		help(error);
		return bx::kExitFailure;
	}

	frost::DefaultAllocator importAllocator;
	frost::Settings settings(&importAllocator);
	frost::FilePath sourcePath;
	const char* configPath = cmdLine.findOption("config");
	if (NULL != configPath)
	{
		frost::FileReader configReader;
		frost::Error configError;
		if (!frost::open(&configReader, configPath, &configError)) { help("Unable to open import description."); return 1; }
		settings.read(&configReader, &configError);
		frost::close(&configReader);
		const frost::StringView source = settings.get("source");
		if (!configError.isOk() || source.isEmpty()) { help("Import description requires a source."); return 1; }
		sourcePath.set(frost::FilePath(configPath).getPath());
		sourcePath.join(source);
	}
	const auto importBool = [&](const char* _key, bool _default)
	{
		const frost::StringView value = settings.get(_key);
		if (!value.isEmpty() && !frost::fromString(&_default, value)) { help("Invalid boolean import setting."); exit(1); }
		return _default;
	};
	const auto importUint = [&](const char* _key, uint32_t _default)
	{
		const frost::StringView value = settings.get(_key);
		if (!value.isEmpty() && !frost::fromString(&_default, value)) { help("Invalid integer import setting."); exit(1); }
		return _default;
	};

	const auto importFloat = [&](const char* _key, float _default)
	{
		const frost::StringView value = settings.get(_key);
		if (!value.isEmpty() && !frost::fromString(&_default, value)) { help("Invalid float import setting."); exit(1); }
		return _default;
	};
	float simplifyRatio = importFloat("simplify", 1.0f);
	float simplifyError = importFloat("simplifyError", 0.01f);
	const char* ratioArg = cmdLine.findOption("simplify");
	const char* errorArg = cmdLine.findOption("simplify-error");
	if ((NULL != ratioArg && !frost::fromString(&simplifyRatio, frost::StringView(ratioArg)))
		|| (NULL != errorArg && !frost::fromString(&simplifyError, frost::StringView(errorArg)))
		|| !frost::isFinite(simplifyRatio) || simplifyRatio <= 0.0f || simplifyRatio > 1.0f
		|| !frost::isFinite(simplifyError) || simplifyError < 0.0f || simplifyError > 1.0f)
	{
		help("Simplification requires ratio in (0, 1] and error in [0, 1].");
		return bx::kExitFailure;
	}

	int32_t positional = 1;

	const char* filePath = cmdLine.findOption('f');
	filePath = NULL != filePath ? filePath : (NULL != configPath ? sourcePath.getCPtr() : cmdLine.getPositional(positional++));
	if (NULL == filePath)
	{
		help("Input file name must be specified.");
		return bx::kExitFailure;
	}

	const char* outFilePath = cmdLine.findOption('o');
	outFilePath = NULL != outFilePath ? outFilePath : cmdLine.getPositional(positional++);
	if (NULL == outFilePath)
	{
		help("Output file name must be specified.");
		return bx::kExitFailure;
	}

	frost::Guid guid;
	const char* guidOption = cmdLine.findOption("guid");
	const frost::StringView guidArg = NULL != guidOption ? frost::StringView(guidOption) : settings.get("guid");
	if (!frost::fromString(&guid, guidArg) || !frost::isValid(guid) )
	{
		help("A valid persistent --guid must be specified.");
		return bx::kExitFailure;
	}

	float scale = 1.0f;
	if (!settings.get("scale").isEmpty() && !frost::fromString(&scale, settings.get("scale"))) { help("Invalid scale."); return 1; }
	const char* scaleArg = cmdLine.findOption('s', "scale");
	if (NULL != scaleArg)
	{
		if (!bx::fromString(&scale, scaleArg) )
		{
			scale = 1.0f;
		}
	}

	bool compress = cmdLine.hasArg('c', "compress") || importBool("compress", false);

	cmdLine.hasArg(s_obbSteps, '\0', "obb");
	s_obbSteps = bx::min(bx::max(s_obbSteps, 1), 90);

	uint32_t packNormal = importUint("packNormal", 0);
	cmdLine.hasArg(packNormal, '\0', "packnormal");

	uint32_t packUv = importUint("packUv", 0);
	cmdLine.hasArg(packUv, '\0', "packuv");

	bool ccw = cmdLine.hasArg("ccw") || importBool("ccw", false);
	bool flipV = cmdLine.hasArg("flipv") || importBool("flipV", false);
	bool hasTangent = cmdLine.hasArg("tangent") || importBool("tangent", false);
	bool hasBc = cmdLine.hasArg("barycentric");

	if (!bx::isFinite(scale) || scale <= 0.0f || packNormal > 1 || packUv > 1) { help("Invalid scale or vertex packing."); return 1; }
	const frost::StringView coordinateSystem = settings.get("coordinateSystem");
	bool validCoordinateSystem = coordinateSystem.isEmpty();
	CoordinateSystem outputCoordinateSystem;
	outputCoordinateSystem.m_handedness = bx::Handedness::Left;
	outputCoordinateSystem.m_forward = Axis::PositiveZ;
	outputCoordinateSystem.m_up = Axis::PositiveY;
	for (uint32_t ii = 0; ii < BX_COUNTOF(s_coordinateSystemMappings); ++ii)
	{
		if (cmdLine.hasArg(s_coordinateSystemMappings[ii].m_param) || 0 == frost::strCmp(coordinateSystem, s_coordinateSystemMappings[ii].m_param))
		{
			validCoordinateSystem = true;
			outputCoordinateSystem = s_coordinateSystemMappings[ii].m_coordinateSystem;
		}
	}

	bx::FileReader fr;
	if (!bx::open(&fr, filePath) )
	{
		bx::printf("Unable to open input file '%s'.", filePath);
		return bx::kExitFailure;
	}

	int64_t parseElapsed = -bx::getHPCounter();
	int64_t triReorderElapsed = 0;

	uint32_t size = (uint32_t)bx::getSize(&fr);
	char* data = new char[size+1];
	size = bx::read(&fr, data, size, bx::ErrorAssert{});
	data[size] = '\0';
	bx::close(&fr);

	Mesh mesh;
	bx::StringView ext = bx::FilePath(filePath).getExt();
	if (0 == bx::strCmpI(ext, ".obj") )
	{
		parseObj(data, size, &mesh, hasBc);
	}
	else if (0 == bx::strCmpI(ext, ".gltf") || 0 == bx::strCmpI(ext, ".glb") )
	{
		parseGltf(data, size, &mesh, hasBc, bx::FilePath(filePath).getPath() );
	}
	else
	{
		bx::printf("Unsupported input file format '%s'.", filePath);
		exit(bx::kExitFailure);
	}

	delete [] data;

	const int64_t now = bx::getHPCounter();
	parseElapsed += now;
	int64_t convertElapsed = -now;

	if (!validCoordinateSystem) { help("Unknown coordinateSystem."); return 1; }
	std::sort(mesh.m_groups.begin(), mesh.m_groups.end(), GroupSortByMaterial() );

	bool changeWinding = ccw;

	if (scale != 1.0f)
	{
		for (Vec3Array::iterator it = mesh.m_positions.begin(), itEnd = mesh.m_positions.end(); it != itEnd; ++it)
		{
			it->x *= scale;
			it->y *= scale;
			it->z *= scale;
		}
	}

	{
		float meshTransform[16];
		mtxCoordinateTransform(meshTransform, mesh.m_coordinateSystem);

		float meshInvTranform[16];
		bx::mtxTranspose(meshInvTranform, meshTransform);

		float outTransform[16];
		mtxCoordinateTransform(outTransform, outputCoordinateSystem);

		float transform[16];
		bx::mtxMul(transform, meshInvTranform, outTransform);

		if (mtxDeterminant(transform) < 0.0f )
		{
			changeWinding = !changeWinding;
		}

		for (TriIndices& triangle : mesh.m_triangles)
		{
			if (!triangle.m_hasTangents) { continue; }
			for (uint32_t ii = 0; ii < 3; ++ii)
			{
				bx::store(triangle.m_tangents[ii], bx::normalize(bx::mul(bx::load<bx::Vec3>(triangle.m_tangents[ii]), transform)));
				triangle.m_tangents[ii][3] *= mtxDeterminant(transform) < 0.0f ? -1.0f : 1.0f;
				if (flipV) { triangle.m_tangents[ii][3] = -triangle.m_tangents[ii][3]; }
			}
		}
		float identity[16];
		bx::mtxIdentity(identity);

		if (0 != bx::memCmp(identity, transform, sizeof(transform) ) )
		{
			for (Vec3Array::iterator it = mesh.m_positions.begin(), itEnd = mesh.m_positions.end(); it != itEnd; ++it)
			{
				*it = bx::mul(*it, transform);
			}

			for (Vec3Array::iterator it = mesh.m_normals.begin(), itEnd = mesh.m_normals.end(); it != itEnd; ++it)
			{
				*it = bx::mul(*it, transform);
			}
		}
	}


	bool hasColor    = false;
	bool hasNormal   = false;
	bool hasTexcoord = false;

	{
		for (TriangleArray::iterator it = mesh.m_triangles.begin(), itEnd = mesh.m_triangles.end(); it != itEnd && !hasTexcoord; ++it)
		{
			for (uint32_t i = 0; i < 3; ++i)
			{
				hasTexcoord |= -1 != it->m_index[i].m_texcoord;
			}
		}

		for (TriangleArray::iterator it = mesh.m_triangles.begin(), itEnd = mesh.m_triangles.end(); it != itEnd && !hasNormal; ++it)
		{
			for (uint32_t i = 0; i < 3; ++i)
			{
				hasNormal |= -1 != it->m_index[i].m_normal;
			}
		}

		if (changeWinding)
		{
			for (TriangleArray::iterator it = mesh.m_triangles.begin(), itEnd = mesh.m_triangles.end(); it != itEnd; ++it)
			{
				bx::swap(it->m_index[1], it->m_index[2]);
				for (uint32_t jj = 0; jj < 4; ++jj) { bx::swap(it->m_tangents[1][jj], it->m_tangents[2][jj]); }
			}
		}
	}

	if (flipV) { for (bx::Vec3& uv : mesh.m_texcoords) { uv.y = 1.0f - uv.y; } }
	if (hasTangent)
	{
		for (const TriIndices& triangle : mesh.m_triangles)
		{
			for (uint32_t ii = 0; ii < 3; ++ii)
			{
				const Index3& index = triangle.m_index[ii];
				if (index.m_normal < 0 || index.m_texcoord < 0
				|| size_t(index.m_normal) >= mesh.m_normals.size()
				|| size_t(index.m_texcoord) >= mesh.m_texcoords.size())
				{
					help("--tangent requires normals and UVs on every triangle vertex.");
					return bx::kExitFailure;
				}
			}
		}
	}

	if (hasTangent) { generateTangents(mesh); }
	bgfx::VertexLayout layout;
	layout.begin();
	layout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);

	if (hasColor)
	{
		layout.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
	}

	if (hasBc)
	{
		layout.add(bgfx::Attrib::Color1, 4, bgfx::AttribType::Uint8, true);
	}

	if (hasTexcoord)
	{
		switch (packUv)
		{
		default:
		case 0:
			layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
			break;

		case 1:
			layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Half);
			break;
		}
	}

	if (hasNormal)
	{
		hasTangent &= hasTexcoord;

		switch (packNormal)
		{
		default:
		case 0:
			layout.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float);
			if (hasTangent)
			{
				layout.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float);
			}
			break;

		case 1:
			layout.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true);
			if (hasTangent)
			{
				layout.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Uint8, true, true);
			}
			break;
		}
	}

	layout.end();

	uint32_t stride = layout.getStride();
	uint8_t* vertexData = new uint8_t[mesh.m_triangles.size() * 3 * stride];
	uint16_t* indexData = new uint16_t[mesh.m_triangles.size() * 3];
	int32_t numVertices = 0;
	int32_t numIndices = 0;

	int32_t writtenPrimitives = 0;
	int32_t writtenVertices = 0;
	int32_t writtenIndices = 0;

	uint8_t* vertices = vertexData;
	uint16_t* indices = indexData;

	const uint32_t tableSize = 65536 * 2;
	const uint32_t hashmod = tableSize - 1;
	uint32_t* table = new uint32_t[tableSize];
	bx::memSet(table, 0xff, tableSize * sizeof(uint32_t) );

	stl::string material = mesh.m_groups.empty() ? "" : mesh.m_groups.begin()->m_material;

	PrimitiveArray primitives;

	bx::FileWriter writer;
	if (!bx::open(&writer, outFilePath) )
	{
		bx::printf("Unable to open output file '%s'.", outFilePath);
		exit(bx::kExitFailure);
	}

	struct MaterialSlot { stl::string name; uint32_t dependency = UINT32_MAX; };
	stl::vector<MaterialSlot> materialSlots;
	stl::vector<frost::AssetDependency> dependencies;
	for (const Group& group : mesh.m_groups)
	{
		bool found = false;
		for (const MaterialSlot& slot : materialSlots) { found |= slot.name == group.m_material; }
		if (found) { continue; }
		MaterialSlot slot;
		slot.name = group.m_material;
		char key[1024];
		bx::snprintf(key, sizeof(key), "materials/%s", 0 == slot.name.size() ? "default" : slot.name.c_str());
		const frost::StringView value = settings.get(key);
		if (!value.isEmpty())
		{
			frost::Guid materialGuid;
			if (!frost::fromString(&materialGuid, value) || !frost::isValid(materialGuid)) { help("Invalid material GUID in import description."); return 1; }
			uint32_t index = 0;
			while (index < dependencies.size() && dependencies[index].guid != materialGuid) { ++index; }
			if (index == dependencies.size()) { dependencies.push_back({ materialGuid, frost::kAssetTypeMaterial }); }
			slot.dependency = index;
		}
		materialSlots.push_back(slot);
	}
	if (materialSlots.empty() || materialSlots.size() > UINT16_MAX || dependencies.size() > UINT16_MAX)
	{ help("Invalid material slot count."); return 1; }
	frost::AssetHeader header;
	header.dependencyCount = uint32_t(dependencies.size());
	header.payloadOffset = frost::kAssetHeaderSize + header.dependencyCount * frost::kAssetDependencySize;
	header.type = frost::kAssetTypeMesh;
	header.payloadVersion = frost::kMeshVersion;
	header.guid = guid;
	uint8_t headerBytes[frost::kAssetHeaderSize];
	frost::writeAssetHeader(headerBytes, sizeof(headerBytes), header, frost::ErrorAssert{});
	bx::Error err;
	bx::write(&writer, headerBytes, sizeof(headerBytes), &err);
	for (const frost::AssetDependency& dependency : dependencies)
	{
		frost::writeAssetDependency(headerBytes, sizeof(headerBytes), dependency, frost::ErrorAssert{});
		bx::write(&writer, headerBytes, frost::kAssetDependencySize, &err);
	}
	bx::writeLE(&writer, uint32_t(0), &err); // Group count, patched after compilation.
	bx::writeLE(&writer, uint16_t(materialSlots.size()), &err);
	for (const MaterialSlot& slot : materialSlots)
	{
		if (slot.name.size() > UINT16_MAX) { help("Material slot name is too long."); return 1; }
		bx::writeLE(&writer, uint16_t(slot.name.size()), &err);
		bx::write(&writer, slot.name.c_str(), uint32_t(slot.name.size()), &err);
		bx::writeLE(&writer, slot.dependency, &err);
	}
	uint32_t numGroups = 0;

	Primitive prim;
	prim.m_startVertex = 0;
	prim.m_startIndex  = 0;

	uint32_t positionOffset = layout.getOffset(bgfx::Attrib::Position);
	uint32_t color0Offset   = layout.getOffset(bgfx::Attrib::Color0);

	Group sentinelGroup;
	sentinelGroup.m_startTriangle = 0;
	sentinelGroup.m_numTriangles = UINT32_MAX;
	mesh.m_groups.push_back(sentinelGroup);

	uint32_t ii = 0;
	for (GroupArray::const_iterator groupIt = mesh.m_groups.begin(); groupIt != mesh.m_groups.end(); ++groupIt, ++ii)
	{
		const bool sentinel = groupIt->m_startTriangle == 0 && groupIt->m_numTriangles == UINT32_MAX;

		for (uint32_t tri = groupIt->m_startTriangle, end = tri + groupIt->m_numTriangles; tri < end; ++tri)
		{
			if (0 != bx::strCmp(material.c_str(), groupIt->m_material.c_str() )
			||  sentinel
			||  65533 <= numVertices)
			{
				prim.m_numVertices = numVertices - prim.m_startVertex;
				prim.m_numIndices  = numIndices  - prim.m_startIndex;

				if (prim.m_startIndex < uint32_t(numIndices) )
				{
					primitives.push_back(prim);
				}


				if (simplifyRatio < 1.0f && numIndices > 0)
				{
					numIndices = int32_t(simplifyMesh(indexData, uint32_t(numIndices), vertexData,
						uint32_t(numVertices), stride, primitives, simplifyRatio, simplifyError));
				}
				triReorderElapsed -= bx::getHPCounter();

				for (PrimitiveArray::const_iterator primIt = primitives.begin(); primIt != primitives.end(); ++primIt)
				{
					const Primitive& prim1 = *primIt;
					optimizeVertexCache(indexData + prim1.m_startIndex, prim1.m_numIndices, numVertices);
				}

				numVertices = optimizeVertexFetch(indexData, numIndices, vertexData, numVertices, uint16_t(stride) );

				triReorderElapsed += bx::getHPCounter();

				if (0 < numVertices
				&&  0 < numIndices)
				{
					uint16_t materialSlot = 0;
					while (materialSlots[materialSlot].name != material) { ++materialSlot; }
					++numGroups;
					write(&writer
						, vertexData
						, numVertices
						, layout
						, indexData
						, numIndices
						, compress
						, materialSlot
						, primitives
						, &err
						);
				}
				primitives.clear();

				bx::memSet(table, 0xff, tableSize * sizeof(uint32_t) );

				++writtenPrimitives;
				writtenVertices += numVertices;
				writtenIndices += numIndices;

				vertices = vertexData;
				indices  = indexData;
				numVertices = 0;
				numIndices  = 0;
				prim.m_startVertex = 0;
				prim.m_startIndex  = 0;

				material = groupIt->m_material;

				if (sentinel)
				{
					break;
				}
			}

			TriIndices& triangle = mesh.m_triangles[tri];
			for (uint32_t edge = 0; edge < 3; ++edge)
			{
				Index3& index = triangle.m_index[edge];

				float* position = (float*)(vertices + positionOffset);
				bx::memCopy(position, &mesh.m_positions[index.m_position], 3*sizeof(float) );

				if (hasColor)
				{
					uint32_t* color0 = (uint32_t*)(vertices + color0Offset);
					*color0 = rgbaToAbgr(numVertices%255, numIndices%255, 0, 0xff);
				}

				if (hasBc)
				{
					const float bc[4] =
					{
						(index.m_vbc == 0) ? 1.0f : 0.0f,
						(index.m_vbc == 1) ? 1.0f : 0.0f,
						(index.m_vbc == 2) ? 1.0f : 0.0f,
						0.0f
					};
					bgfx::vertexPack(bc, true, bgfx::Attrib::Color1, layout, vertices);
				}

				if (hasTexcoord)
				{
					float uv[2];
					bx::memCopy(uv, &mesh.m_texcoords[index.m_texcoord == -1 ? 0 : index.m_texcoord], 2*sizeof(float) );


					bgfx::vertexPack(uv, true, bgfx::Attrib::TexCoord0, layout, vertices);
				}

				if (hasNormal)
				{
					float normal[4];
					bx::store(normal, bx::normalize(bx::load<bx::Vec3>(&mesh.m_normals[index.m_normal == -1 ? 0 : index.m_normal]) ) );
					normal[3] = 0.0f;
					bgfx::vertexPack(normal, true, bgfx::Attrib::Normal, layout, vertices);
				}

				if (hasTangent) { bgfx::vertexPack(triangle.m_tangents[edge], true, bgfx::Attrib::Tangent, layout, vertices); }
				uint32_t hash = bx::hash<bx::HashMurmur2A>(vertices, stride);
				size_t bucket = hash & hashmod;
				uint32_t vertexIndex = UINT32_MAX;

				for (size_t probe = 0; probe <= hashmod; ++probe)
				{
					uint32_t& item = table[bucket];

					if (item == ~0u)
					{
						vertices += stride;
						item = numVertices++;
						vertexIndex = item;
						break;
					}

					if (0 == bx::memCmp(vertexData + item * stride, vertices, stride) )
					{
						vertexIndex = item;
						break;
					}

					bucket = (bucket + probe + 1) & hashmod;
				}

				if ( vertexIndex == UINT32_MAX )
				{
					bx::printf("hash table insert failed");
					exit(bx::kExitFailure);
				}

				*indices++ = (uint16_t)vertexIndex;
				++numIndices;
			}
		}

		prim.m_numVertices = numVertices - prim.m_startVertex;
		if (prim.m_startIndex < uint32_t(numIndices) )
		{
			prim.m_numIndices = numIndices - prim.m_startIndex;
			prim.m_name = groupIt->m_name;
			primitives.push_back(prim);
			prim.m_startVertex = numVertices;
			prim.m_startIndex  = numIndices;
		}

		BX_TRACE("%3d: s %5d, n %5d, %s\n"
			, ii
			, groupIt->m_startTriangle
			, groupIt->m_numTriangles
			, groupIt->m_material.c_str()
			);
	}

	BX_ASSERT(0 == primitives.size(), "Not all primitives are written");

	const int64_t outputSize = bx::seek(&writer);
	if (0 == numGroups || outputSize > UINT32_MAX)
	{
		BX_ERROR_SET(&err, bx::kErrorReaderWriterWrite, "Mesh is empty or exceeds the asset size limit.");
	}
	header.payloadSize = uint32_t(outputSize - header.payloadOffset);
	frost::writeAssetHeader(headerBytes, sizeof(headerBytes), header, frost::ErrorAssert{});
	bx::seek(&writer, 0, bx::Whence::Begin);
	bx::write(&writer, headerBytes, sizeof(headerBytes), &err);
	bx::seek(&writer, header.payloadOffset, bx::Whence::Begin);
	bx::writeLE(&writer, numGroups, &err);
	bx::printf("size: %d\n", uint32_t(outputSize) );
	bx::close(&writer);
	if (!err.isOk() )
	{
		bx::remove(outFilePath);
		bx::printf("Unable to write mesh asset: %s\n", err.getMessage().getCPtr() );
	}

	delete [] table;
	delete [] indexData;
	delete [] vertexData;

	convertElapsed += bx::getHPCounter();

	bx::printf("parse %f [s]\ntri reorder %f [s]\nconvert %f [s]\ng %d, p %d, v %d, i %d\n"
		, double(parseElapsed)/bx::getHPFrequency()
		, double(triReorderElapsed)/bx::getHPFrequency()
		, double(convertElapsed)/bx::getHPFrequency()
		, uint32_t(mesh.m_groups.size()-1)
		, writtenPrimitives
		, writtenVertices
		, writtenIndices
		);

	return err.isOk() ? bx::kExitSuccess : bx::kExitFailure;
}
