/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "pbr.sh"
#define FROST_DDGI_SAMPLE 1
#include "ddgi.sh"
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

// Recover the world hit from the existing inverse instance transform and barycentrics.
vec3 rayHitPosition(RayHit _hit)
{
	uvec4 primitive = floatBitsToUint(b_rayTriangles[_hit.primitive]);
	vec3 a = b_rayVertices[primitive.x * 2u].xyz;
	vec3 b = b_rayVertices[primitive.y * 2u].xyz;
	vec3 c = b_rayVertices[primitive.z * 2u].xyz;
	uint base = _hit.instanceIndex * 4u;
	vec4 r0 = b_rayInstances[base], r1 = b_rayInstances[base + 1u], r2 = b_rayInstances[base + 2u];
	vec3 local = a + _hit.barycentric.x * (b - a) + _hit.barycentric.y * (c - a) - vec3(r0.w, r1.w, r2.w);
	vec3 cofactor = cross(r1.xyz, r2.xyz);
	return (cofactor * local.x + cross(r2.xyz, r0.xyz) * local.y + cross(r0.xyz, r1.xyz) * local.z) / dot(r0.xyz, cofactor);
}

vec3 shadeRayHit(vec3 _position, vec3 _direction, RayHit _hit, uint _numLights, float _roughness, float _radius, bool _diffuseRay)
{
	uvec4 primitive = floatBitsToUint(b_rayTriangles[_hit.primitive]);
	uint base = _hit.instanceIndex * 4u;
	vec4 info = b_rayInstances[base + 3u];
	uint material = (uint(info.y) + primitive.w) * 2u;
	vec4 properties = b_rayMaterials[material], samplers = b_rayMaterials[material + 1u];
	vec2 uv = rayHitUv(primitive, _hit.barycentric);
	float mip = max(_diffuseRay ? 2.0 : 0.0, rayMaterialLod(_hit, _direction, _radius));
	vec3 baseColor = texture2DArrayLod(s_rayBaseColor, vec3(rayUv(uv, samplers.x), properties.x), mip).rgb;
	vec2 parameters = texture2DArrayLod(s_rayParameters, vec3(rayUv(uv, samplers.y), properties.x), mip).rg;
	// A rough ray cannot resolve a sharper secondary specular lobe.
	parameters.y = max(parameters.y, _roughness);
	vec3 color = texture2DArrayLod(s_rayEmissive, vec3(rayUv(uv, samplers.z), properties.x), mip).rgb * u_reflectionSettings.w;
	vec3 a = b_rayVertices[primitive.x * 2u + 1u].xyz;
	vec3 b = b_rayVertices[primitive.y * 2u + 1u].xyz;
	vec3 c = b_rayVertices[primitive.z * 2u + 1u].xyz;
	vec3 n = a + _hit.barycentric.x * (b - a) + _hit.barycentric.y * (c - a);
	vec3 normal = normalize(b_rayInstances[base].xyz * n.x + b_rayInstances[base + 1u].xyz * n.y + b_rayInstances[base + 2u].xyz * n.z);
	vec3 geometric = rayHitGeometricNormal(_hit);
	// Backfaces still block the environment; only double-sided materials shade their back.
	if (dot(geometric, _direction) > 0.0)
	{
		if (properties.z < 0.5) { return vec3_splat(0.0); }
		normal = -normal;
		geometric = -geometric;
	}
	vec3 position = _position;
	vec3 view = -_direction;
	// Probe rays integrate broad lobes. Fold secondary specular energy into diffuse
	// instead of retaining a noisy, view-dependent highlight (DDGI, section 6).
	vec3 f0 = mix(vec3_splat(0.04), baseColor, parameters.x);
	vec3 fresnelAverage = f0 + (vec3_splat(1.0) - f0) / 21.0;
	vec3 reflectance = baseColor * (1.0 - parameters.x) * (vec3_splat(1.0) - fresnelAverage) + fresnelAverage;
	for (uint ii = 0u; ii < _numLights; ++ii)
	{
		uint light = ii * 8u;
		vec4 location = b_rayLights[light], emission = b_rayLights[light + 1u];
		if (dot(emission.rgb, emission.rgb) <= 0.0) { continue; }
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
					vec3 response = _diffuseRay ? reflectance * saturate(dot(normal, sampleDirection)) / 3.14159265359
						: evaluatePbr(normal, view, sampleDirection, baseColor, parameters.x, parameters.y);
					contribution += response
						* max(0.0, facing) / max(sampleDistance2, shape.y * shape.y);
				}
			}
			contribution *= right.w * up.w * 0.25;
		}
		else
		{
			contribution = _diffuseRay ? reflectance * saturate(dot(normal, direction)) / 3.14159265359
				: evaluatePbr(normal, view, direction, baseColor, parameters.x, parameters.y);
			if (emission.w >= 0.0) { contribution /= max(distance2, shape.y * shape.y); }
		}
		if (dot(contribution, contribution) <= 0.0) { continue; }
		float visibility = 1.0;
		if (b_rayLights[light + 7u].x > 0.5)
		{
			// A world-space irradiance field must not depend on the camera's CSM
			// coverage, cascade resolution, or the visible local-shadow allocation.
			if (_diffuseRay) { visibility = rayVisibility(position, geometric, direction, distance); }
			else
			{
#if defined(FROST_RAY_CAPTURE)
				vec4 shadow = b_rayLights[light + 6u];
				// Static cube shadows remain available independently of camera visibility.
				BRANCH if (shadow.x >= 0.0 && emission.w >= 0.0 && b_rayLights[light + 7u].z > 0.5)
				{
					visibility = reflectedPointVisibility(position, geometric, location, shape, shadow);
				}
				else { visibility = rayVisibility(position, geometric, direction, distance); }
#else
				vec4 shadow = b_rayLights[light + 6u];
				BRANCH if (emission.w < 0.0 && b_rayLights[light + 7u].y >= 0.0 && shadowCoverage(position, geometric, int(b_rayLights[light + 7u].y)))
				{
					visibility = shadowVisibility(position, geometric, int(b_rayLights[light + 7u].y));
				}
				else
				{
					BRANCH if (shadow.x >= 0.0)
					{
						visibility = reflectedPointVisibility(position, geometric, location, shape, shadow);
					}
					else
					{
						visibility = rayVisibility(position, geometric, direction, distance);
					}
				}
#endif
			}
		}
		color += contribution * emission.rgb * attenuation * visibility;
	}
	vec3 irradiance = sampleDiffuseGi(position, normal, geometric, view);
	if (_diffuseRay) { color += irradiance * reflectance; }
	else
	{
		color += evaluateIndirectDiffuse(irradiance, baseColor, parameters.x);
		// DDGI supplies the low-frequency secondary glossy energy without recursive mirror rays.
		color += evaluateIndirectSpecular(irradiance, saturate(dot(normal, view)), f0, max(0.6, parameters.y));
	}
	return color;
}

