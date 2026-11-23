/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

BUFFER_RO(b_rayInstancesBvh, vec4, 2);
BUFFER_RO(b_rayGeometryBvh, vec4, 3);
BUFFER_RO(b_rayVertices, vec4, 4);
BUFFER_RO(b_rayTriangles, vec4, 5);
BUFFER_RO(b_rayInstances, vec4, 6);
BUFFER_RO(b_rayMaterials, vec4, 7);
SAMPLER2DARRAY(s_rayBaseColor, 9);
SAMPLER2DARRAY(s_rayParameters, 10);
SAMPLER2DARRAY(s_rayEmissive, 11);
uniform vec4 u_rayScene; // Instance count, material texture size, origin bias, pixel projection scale.

struct RayHit
{
	float distance;
	uint primitive;
	uint instanceIndex;
	vec2 barycentric;
};

vec3 rayHitGeometricNormal(RayHit _hit)
{
	uvec4 primitive = floatBitsToUint(b_rayTriangles[_hit.primitive]);
	vec3 a = b_rayVertices[primitive.x * 2u].xyz;
	vec3 n = cross(b_rayVertices[primitive.y * 2u].xyz - a, b_rayVertices[primitive.z * 2u].xyz - a);
	uint base = _hit.instanceIndex * 4u;
	vec3 r0 = b_rayInstances[base].xyz, r1 = b_rayInstances[base + 1u].xyz, r2 = b_rayInstances[base + 2u].xyz;
	float orientation = dot(r0, cross(r1, r2)) < 0.0 ? -1.0 : 1.0;
	return normalize((r0 * n.x + r1 * n.y + r2 * n.z) * orientation);
}

bool rayHitBackface(RayHit _hit, vec3 _direction)
{
	uvec4 primitive = floatBitsToUint(b_rayTriangles[_hit.primitive]);
	uint material = uint(b_rayInstances[_hit.instanceIndex * 4u + 3u].y) + primitive.w;
	return b_rayMaterials[material * 2u].z < 0.5 && dot(rayHitGeometricNormal(_hit), _direction) > 0.0;
}

vec3 rayWorldVector(uint _base, vec3 _local)
{
	vec3 r0 = b_rayInstances[_base].xyz, r1 = b_rayInstances[_base + 1u].xyz, r2 = b_rayInstances[_base + 2u].xyz;
	vec3 cofactor = cross(r1, r2);
	return (cofactor * _local.x + cross(r2, r0) * _local.y + cross(r0, r1) * _local.z) / dot(r0, cofactor);
}

float rayMaterialLod(RayHit _hit, vec3 _direction, float _radius)
{
	uvec4 primitive = floatBitsToUint(b_rayTriangles[_hit.primitive]);
	vec4 a = b_rayVertices[primitive.x * 2u], b = b_rayVertices[primitive.y * 2u], c = b_rayVertices[primitive.z * 2u];
	vec2 uv0 = vec2(a.w, b_rayVertices[primitive.x * 2u + 1u].w);
	vec2 uv1 = vec2(b.w, b_rayVertices[primitive.y * 2u + 1u].w) - uv0;
	vec2 uv2 = vec2(c.w, b_rayVertices[primitive.z * 2u + 1u].w) - uv0;
	uint base = _hit.instanceIndex * 4u;
	vec3 area = cross(rayWorldVector(base, b.xyz - a.xyz), rayWorldVector(base, c.xyz - a.xyz));
	float density = sqrt(abs(uv1.x * uv2.y - uv1.y * uv2.x) / max(length(area), 0.00000001));
	float grazing = max(0.1, abs(dot(rayHitGeometricNormal(_hit), _direction)));
	return clamp(log2(max(1.0, 2.0 * _radius * density * u_rayScene.y / grazing)), 0.0, log2(u_rayScene.y));
}

vec2 rayUv(vec2 _uv, float _sampler)
{
	uint flags = uint(_sampler);
	vec2 uv = fract(_uv);
	if ((flags & 1u) != 0u) { uv.x = clamp(_uv.x, 0.5 / u_rayScene.y, 1.0 - 0.5 / u_rayScene.y); }
	else if ((flags & 4u) != 0u) { uv.x = 1.0 - abs(fract(_uv.x * 0.5) * 2.0 - 1.0); }
	if ((flags & 2u) != 0u) { uv.y = clamp(_uv.y, 0.5 / u_rayScene.y, 1.0 - 0.5 / u_rayScene.y); }
	else if ((flags & 8u) != 0u) { uv.y = 1.0 - abs(fract(_uv.y * 0.5) * 2.0 - 1.0); }
	if ((flags & 16u) != 0u) { uv = (floor(uv * u_rayScene.y) + 0.5) / u_rayScene.y; }
	return uv;
}

float rayBox(vec3 _origin, vec3 _inverse, vec3 _minimum, vec3 _maximum, float _limit)
{
	vec3 a = (_minimum - _origin) * _inverse;
	vec3 b = (_maximum - _origin) * _inverse;
	vec3 lo = min(a, b), hi = max(a, b);
	float entry = max(max(lo.x, lo.y), max(lo.z, 0.0));
	float exit = min(min(hi.x, hi.y), min(hi.z, _limit));
	return entry <= exit ? entry : 1e30;
}

vec3 rayInverse(vec3 _direction)
{
	// Finite reciprocals also handle rays parallel to a box face without 0 * infinity.
	return vec3(_direction.x < 0.0 ? -1.0 : 1.0, _direction.y < 0.0 ? -1.0 : 1.0,
		_direction.z < 0.0 ? -1.0 : 1.0) / max(abs(_direction), vec3_splat(1e-20));
}

vec2 rayHitUv(uvec4 _triangle, vec2 _barycentric)
{
	vec2 a = vec2(b_rayVertices[_triangle.x * 2u].w, b_rayVertices[_triangle.x * 2u + 1u].w);
	vec2 b = vec2(b_rayVertices[_triangle.y * 2u].w, b_rayVertices[_triangle.y * 2u + 1u].w);
	vec2 c = vec2(b_rayVertices[_triangle.z * 2u].w, b_rayVertices[_triangle.z * 2u + 1u].w);
	return a + _barycentric.x * (b - a) + _barycentric.y * (c - a);
}

bool traceGeometry(vec3 _origin, vec3 _direction, uint _instance, bool _shadow, inout RayHit _hit)
{
	uint base = _instance * 4u;
	vec4 info = b_rayInstances[base + 3u];
	if (_shadow && info.z < 0.5) { return false; }
	vec4 r0 = b_rayInstances[base], r1 = b_rayInstances[base + 1u], r2 = b_rayInstances[base + 2u];
	vec3 origin = vec3(dot(r0, vec4(_origin, 1.0)), dot(r1, vec4(_origin, 1.0)), dot(r2, vec4(_origin, 1.0)));
	// Keep the transformed direction unnormalized so distance remains in world units.
	vec3 direction = vec3(dot(r0.xyz, _direction), dot(r1.xyz, _direction), dot(r2.xyz, _direction));
	vec3 inverse = rayInverse(direction);
	uint stack[64];
	uint size = 0u, node = uint(info.x);
	bool found = false;
	for (;;)
	{
		vec4 lo = b_rayGeometryBvh[node * 2u], hi = b_rayGeometryBvh[node * 2u + 1u];
		if (rayBox(origin, inverse, lo.xyz, hi.xyz, _hit.distance) < 1e30)
		{
			uint first = floatBitsToUint(lo.w), count = floatBitsToUint(hi.w);
			if (count == 0u)
			{
				vec4 a = b_rayGeometryBvh[first * 2u], b = b_rayGeometryBvh[(first + 1u) * 2u];
				float da = rayBox(origin, inverse, a.xyz, b_rayGeometryBvh[first * 2u + 1u].xyz, _hit.distance);
				float db = rayBox(origin, inverse, b.xyz, b_rayGeometryBvh[(first + 1u) * 2u + 1u].xyz, _hit.distance);
				if (min(da, db) < 1e30)
				{
					if (max(da, db) < 1e30) { stack[size++] = da < db ? first + 1u : first; }
					node = da < db ? first : first + 1u;
					continue;
				}
			}
			else
			{
				for (uint ii = 0u; ii < count; ++ii)
				{
					uvec4 primitive = floatBitsToUint(b_rayTriangles[first + ii]);
					vec3 a = b_rayVertices[primitive.x * 2u].xyz;
					vec3 e1 = b_rayVertices[primitive.y * 2u].xyz - a;
					vec3 e2 = b_rayVertices[primitive.z * 2u].xyz - a;
					vec3 p = cross(direction, e2);
					float determinant = dot(e1, p);
					if (abs(determinant) < 1e-12) { continue; }
					vec3 delta = origin - a;
					float u = dot(delta, p) / determinant;
					if (u < 0.0 || u > 1.0) { continue; }
					vec3 q = cross(delta, e1);
					float v = dot(direction, q) / determinant;
					if (v < 0.0 || u + v > 1.0) { continue; }
					float distance = dot(e2, q) / determinant;
					if (distance <= 0.0 || distance >= _hit.distance) { continue; }
					uint material = uint(info.y) + primitive.w;
					vec4 properties = b_rayMaterials[material * 2u];
					if (properties.y >= 0.0)
					{
						vec2 uv = rayUv(rayHitUv(primitive, vec2(u, v)), b_rayMaterials[material * 2u + 1u].x);
						if (texture2DArrayLod(s_rayBaseColor, vec3(uv, properties.x), 0.0).a < properties.y) { continue; }
					}
					_hit.distance = distance;
					_hit.primitive = first + ii;
					_hit.instanceIndex = _instance;
					_hit.barycentric = vec2(u, v);
					found = true;
					if (_shadow) { return true; }
				}
			}
		}
		if (size == 0u) { break; }
		node = stack[--size];
	}
	return found;
}

bool traceRay(vec3 _origin, vec3 _direction, bool _shadow, inout RayHit _hit)
{
	if (u_rayScene.x < 0.5) { return false; }
	vec3 inverse = rayInverse(_direction);
	uint stack[64];
	uint size = 0u, node = 0u;
	bool found = false;
	for (;;)
	{
		vec4 lo = b_rayInstancesBvh[node * 2u], hi = b_rayInstancesBvh[node * 2u + 1u];
		if (rayBox(_origin, inverse, lo.xyz, hi.xyz, _hit.distance) < 1e30)
		{
			uint first = floatBitsToUint(lo.w);
			if (floatBitsToUint(hi.w) == 0u)
			{
				vec4 a = b_rayInstancesBvh[first * 2u], b = b_rayInstancesBvh[(first + 1u) * 2u];
				float da = rayBox(_origin, inverse, a.xyz, b_rayInstancesBvh[first * 2u + 1u].xyz, _hit.distance);
				float db = rayBox(_origin, inverse, b.xyz, b_rayInstancesBvh[(first + 1u) * 2u + 1u].xyz, _hit.distance);
				if (min(da, db) < 1e30)
				{
					if (max(da, db) < 1e30) { stack[size++] = da < db ? first + 1u : first; }
					node = da < db ? first : first + 1u;
					continue;
				}
			}
			else if (traceGeometry(_origin, _direction, first, _shadow, _hit))
			{
				found = true;
				if (_shadow) { return true; }
			}
		}
		if (size == 0u) { break; }
		node = stack[--size];
	}
	return found;
}

float rayVisibility(vec3 _position, vec3 _normal, vec3 _direction, float _distance)
{
	RayHit hit;
	hit.distance = max(0.0, _distance - u_rayScene.z * 2.0);
	return traceRay(_position + _normal * u_rayScene.z, _direction, true, hit) ? 0.0 : 1.0;
}
