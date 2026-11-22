/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "pbr.sh"
BUFFER_RO(b_rayLights, vec4, 8);
SAMPLERCUBEARRAY(s_pointShadows, 1);
#if !defined(FROST_RAY_CAPTURE)
#define FROST_SHADOW_SAMPLER 12
#include "shadow.sh"
#endif

float reflectedPointVisibility(vec3 _position, vec3 _normal, vec4 _location, vec4 _shape, vec4 _shadow)
{
	vec3 delta = _position + _normal * _shadow.z - _location.xyz;
	float depth = max(abs(delta.x), max(abs(delta.y), abs(delta.z))) - _shadow.y;
	float compare = (_location.w - _shape.y * _location.w / max(depth, _shape.y)) / (_location.w - _shape.y);
	vec3 direction = normalize(delta);
	vec3 up = abs(direction.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, direction));
	up = cross(direction, right);
	float visibility = 0.0;
	for (int yy = 0; yy < 2; ++yy)
	{
		for (int xx = 0; xx < 2; ++xx)
		{
			vec3 offset = (right * (float(xx) - 0.5) + up * (float(yy) - 0.5)) * _shadow.w * 2.0;
			visibility += step(compare, textureCubeArrayLod(s_pointShadows, vec4(direction + offset, _shadow.x), 0.0).r);
		}
	}
	return visibility * 0.25;
}
vec3 shadeRayHit(vec3 _origin, vec3 _direction, RayHit _hit, uint _numLights)
{
	uvec4 primitive = floatBitsToUint(b_rayTriangles[_hit.primitive]);
	uint base = _hit.instanceIndex * 4u;
	vec4 info = b_rayInstances[base + 3u];
	uint material = (uint(info.y) + primitive.w) * 2u;
	vec4 properties = b_rayMaterials[material], samplers = b_rayMaterials[material + 1u];
	vec2 uv = rayHitUv(primitive, _hit.barycentric);
	vec3 baseColor = texture2DArrayLod(s_rayBaseColor, vec3(rayUv(uv, samplers.x), properties.x), 0.0).rgb;
	vec2 parameters = texture2DArrayLod(s_rayParameters, vec3(rayUv(uv, samplers.y), properties.x), 0.0).rg;
	vec3 color = texture2DArrayLod(s_rayEmissive, vec3(rayUv(uv, samplers.z), properties.x), 0.0).rgb;
	vec3 a = b_rayVertices[primitive.x * 2u + 1u].xyz;
	vec3 b = b_rayVertices[primitive.y * 2u + 1u].xyz;
	vec3 c = b_rayVertices[primitive.z * 2u + 1u].xyz;
	vec3 n = a + _hit.barycentric.x * (b - a) + _hit.barycentric.y * (c - a);
	vec3 normal = normalize(b_rayInstances[base].xyz * n.x + b_rayInstances[base + 1u].xyz * n.y + b_rayInstances[base + 2u].xyz * n.z);
	// Backfaces still block the environment; only double-sided materials shade their back.
	if (dot(normal, _direction) > 0.0)
	{
		if (properties.z < 0.5) { return vec3_splat(0.0); }
		normal = -normal;
	}
	vec3 position = _origin + _direction * _hit.distance;
	vec3 view = -_direction;
	for (uint ii = 0u; ii < _numLights; ++ii)
	{
		uint light = ii * 8u;
		vec4 location = b_rayLights[light], emission = b_rayLights[light + 1u];
		vec3 delta = location.xyz - position;
		float distance2 = dot(delta, delta);
		float attenuation = 1.0;
		vec3 direction = location.xyz;
		float distance = 1e30;
		if (emission.w >= 0.0)
		{
			if (distance2 >= location.w * location.w) { continue; }
			distance = sqrt(max(distance2, 1e-12));
			direction = delta / distance;
			float ratio = distance2 / (location.w * location.w);
			attenuation = saturate(1.0 - ratio * ratio);
			attenuation *= attenuation;
			if (emission.w > 0.5 && emission.w < 1.5)
			{
				vec4 axis = b_rayLights[light + 2u], shape = b_rayLights[light + 5u];
				float cone = saturate((dot(-direction, axis.xyz) - axis.w) / max(shape.x - axis.w, 0.0001));
				attenuation *= cone * cone;
			}
		}
		if (attenuation <= 0.0) { continue; }
		if (emission.w < 1.5 && dot(normal, direction) <= 0.0) { continue; }
		vec4 shape = b_rayLights[light + 5u];
		vec3 contribution = vec3_splat(0.0);
		if (emission.w > 1.5)
		{
			vec4 axis = b_rayLights[light + 2u], right = b_rayLights[light + 3u], up = b_rayLights[light + 4u];
			for (int yy = 0; yy < 4; ++yy)
			{
				for (int xx = 0; xx < 4; ++xx)
				{
					vec3 sampleDelta = delta + right.xyz * right.w * ((float(xx) + 0.5) * 0.5 - 1.0)
						+ up.xyz * up.w * ((float(yy) + 0.5) * 0.5 - 1.0);
					float sampleDistance2 = max(dot(sampleDelta, sampleDelta), 1e-6);
					vec3 sampleDirection = sampleDelta * inversesqrt(sampleDistance2);
					float facing = dot(-sampleDirection, axis.xyz);
					if (shape.z > 0.5) { facing = abs(facing); }
					contribution += evaluatePbr(normal, view, sampleDirection, baseColor, parameters.x, parameters.y)
						* max(0.0, facing) / max(sampleDistance2, shape.y * shape.y);
				}
			}
			contribution *= right.w * up.w * 0.25;
		}
		else
		{
			contribution = evaluatePbr(normal, view, direction, baseColor, parameters.x, parameters.y);
			if (emission.w >= 0.0) { contribution /= max(distance2, shape.y * shape.y); }
		}
		if (dot(contribution, contribution) <= 0.0) { continue; }
		float visibility = 1.0;
		if (b_rayLights[light + 7u].x > 0.5)
		{
#if defined(FROST_RAY_CAPTURE)
			vec4 shadow = b_rayLights[light + 6u];
			// Static cube shadows remain available independently of camera visibility.
			BRANCH if (shadow.x >= 0.0 && emission.w >= 0.0 && b_rayLights[light + 7u].z > 0.5)
			{
				visibility = reflectedPointVisibility(position, normal, location, shape, shadow);
			}
			else { visibility = rayVisibility(position, normal, direction, distance); }
#else
			vec4 shadow = b_rayLights[light + 6u];
			BRANCH if (emission.w < 0.0 && b_rayLights[light + 7u].y > 0.5 && shadowCoverage(position, normal))
			{
				visibility = shadowVisibility(position, normal);
			}
			else
			{
				BRANCH if (shadow.x >= 0.0)
				{
					visibility = reflectedPointVisibility(position, normal, location, shape, shadow);
				}
				else
				{
					visibility = rayVisibility(position, normal, direction, distance);
				}
			}
#endif
		}
		color += contribution * emission.rgb * attenuation * visibility;
	}
	// DDGI can add visibility-aware diffuse illumination here without changing traversal.
	return color;
}

