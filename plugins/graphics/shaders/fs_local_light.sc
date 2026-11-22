/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

$input v_texcoord0

#include "../common/common.sh"
#include "pbr.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_albedo, 1);
SAMPLER2D(s_material, 2);
SAMPLER2D(s_depth, 4);
SAMPLERCUBEARRAY(s_pointShadows, 6);
SAMPLER2DARRAY(s_spotShadows, 7);
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_localLight[8];
uniform mat4 u_localProjection;

float localVisibility(vec3 _position, vec3 _normal)
{
	if (u_localLight[6].x < 0.0) { return 1.0; }
	vec3 delta = _position + _normal * u_localLight[6].z - u_localLight[0].xyz;
	float depth = max(abs(delta.x), max(abs(delta.y), abs(delta.z))) - u_localLight[6].y;
	float nearPlane = u_localLight[5].y;
	float farPlane = u_localLight[0].w;
	float visibility = 0.0;
	if (u_localLight[1].w > 0.5 && u_localLight[1].w < 1.5)
	{
		vec4 clip = mul(u_localProjection, vec4(_position + _normal * u_localLight[6].z, 1.0));
		vec2 uv = clip.xy / clip.w;
		uv.y = toClipSpaceY(uv.y);
		uv = uv * 0.5 + 0.5;
		if (any(lessThan(uv, vec2_splat(0.0))) || any(greaterThan(uv, vec2_splat(1.0)))) { return 1.0; }
		depth = dot(delta, u_localLight[2].xyz) - u_localLight[6].y;
		for (int yy = -1; yy <= 1; ++yy)
		{
			for (int xx = -1; xx <= 1; ++xx)
			{
				vec2 offset = vec2(float(xx), float(yy)) * u_localLight[6].w;
				float sampleDepth = texture2DArrayLod(s_spotShadows, vec3(clamp(uv + offset,
					vec2_splat(0.5 * u_localLight[6].w), vec2_splat(1.0 - 0.5 * u_localLight[6].w)), u_localLight[6].x), 0.0).r;
				visibility += step((farPlane - nearPlane * farPlane / max(depth, nearPlane)) / (farPlane - nearPlane), sampleDepth);
			}
		}
	}
	else
	{
		vec3 direction = normalize(delta);
		vec3 up = vec3(0.0, 1.0, 0.0);
		if (abs(direction.y) > 0.99) { up = vec3(1.0, 0.0, 0.0); }
		vec3 right = normalize(cross(up, direction));
		up = cross(direction, right);
		for (int yy = -1; yy <= 1; ++yy)
		{
			for (int xx = -1; xx <= 1; ++xx)
			{
				vec3 offset = (right * float(xx) + up * float(yy)) * u_localLight[6].w * 2.0;
				float sampleDepth = textureCubeArrayLod(s_pointShadows, vec4(direction + offset, u_localLight[6].x), 0.0).r;
				visibility += step((farPlane - nearPlane * farPlane / max(depth, nearPlane)) / (farPlane - nearPlane), sampleDepth);
			}
		}
	}
	return visibility / 9.0;
}

void main()
{
	vec4 encoded = texture2D(s_normal, v_texcoord0);
	if (encoded.w < 0.5) { discard; }
	vec3 clip = vec3(v_texcoord0 * 2.0 - 1.0, toClipSpaceDepth(texture2D(s_depth, v_texcoord0).r));
	clip.y = toClipSpaceY(clip.y);
	vec3 position = clipToWorld(u_invSceneViewProj, clip);
	vec3 delta = u_localLight[0].xyz - position;
	float distance2 = dot(delta, delta);
	float range2 = u_localLight[0].w * u_localLight[0].w;
	if (distance2 >= range2) { discard; }
	vec3 direction = delta * inversesqrt(max(distance2, 0.000001));
	float attenuation = saturate(1.0 - distance2 * distance2 / (range2 * range2));
	attenuation *= attenuation;
	if (u_localLight[1].w > 0.5 && u_localLight[1].w < 1.5)
	{
		float angle = (dot(-direction, u_localLight[2].xyz) - u_localLight[2].w)
			/ max(u_localLight[5].x - u_localLight[2].w, 0.0001);
		attenuation *= saturate(angle) * saturate(angle);
	}
	if (attenuation <= 0.0) { discard; }
	vec3 normal = normalize(decodeNormalUint(encoded.xyz));
	vec3 view = normalize(mix(u_sceneCamera.xyz - position, -u_sceneDirection.xyz, u_sceneCamera.w));
	vec3 baseColor = texture2D(s_albedo, v_texcoord0).rgb;
	vec3 parameters = texture2D(s_material, v_texcoord0).rgb;
	vec3 color = vec3_splat(0.0);
	float radius2 = u_localLight[5].y * u_localLight[5].y;
	if (u_localLight[1].w > 1.5)
	{
		// Integrate a finite rectangle using stratified area quadrature. No point-light substitute.
		for (int yy = 0; yy < 4; ++yy)
		{
			for (int xx = 0; xx < 4; ++xx)
			{
				vec3 sampleDelta = delta + u_localLight[3].xyz * u_localLight[3].w * ((float(xx) + 0.5) * 0.5 - 1.0)
					+ u_localLight[4].xyz * u_localLight[4].w * ((float(yy) + 0.5) * 0.5 - 1.0);
				float sampleDistance2 = max(dot(sampleDelta, sampleDelta), 0.000001);
				vec3 sampleDirection = sampleDelta * inversesqrt(sampleDistance2);
				float facing = dot(-sampleDirection, u_localLight[2].xyz);
				if (u_localLight[5].z > 0.5) { facing = abs(facing); }
				color += evaluatePbr(normal, view, sampleDirection, baseColor, parameters.r, parameters.g)
					* max(0.0, facing) / max(sampleDistance2, radius2);
			}
		}
		color *= u_localLight[3].w * u_localLight[4].w * 0.25;
	}
	else
	{
		color = evaluatePbr(normal, view, direction, baseColor, parameters.r, parameters.g) / max(distance2, radius2);
	}
	gl_FragColor = vec4(color * u_localLight[1].rgb * attenuation * localVisibility(position, normal), 0.0);
}
