/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
$input v_texcoord0
#include "../common/common.sh"
#include "reflection.sh"
#include "tonemap.sh"
#include "environment.sh"
#include "ddgi.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_albedo, 1);
SAMPLER2D(s_material, 2);
SAMPLER2D(s_emissive, 3);
SAMPLER2D(s_depth, 4);
SAMPLER2D(s_reflectionSurface, 5);
SAMPLER2D(s_graphicsDebug, 6);
SAMPLERCUBE(s_graphicsDebugSky, 7);
SAMPLERCUBEARRAY(s_pointShadows, 8);
SAMPLER2DARRAY(s_spotShadows, 9);
SAMPLER2DARRAY(s_shadowAtlas, 10);
SAMPLER2D(s_roughness, 11);
uniform mat4 u_invSceneViewProj;
uniform mat4 u_shadowView[2];
uniform vec4 u_shadowSelection;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_shadowSplits[2];
uniform vec4 u_shadowInfo[2];
uniform vec4 u_graphicsDebug; // GraphicsDebugView, world-space depth range, mip, source valid.
uniform vec4 u_debugSource; // Source width/height, sampled UV extent.
uniform vec4 u_debugProbe; // Volume maximum distance, viewport aspect, unused.
uniform vec4 u_debugSky;
uniform vec4 u_localShadowTypes[8];
uniform vec4 u_outputParameters;

vec3 debugHdr(vec3 _color)
{
	vec3 color = max(_color, vec3_splat(0.0)) * u_outputParameters.x;
	return u_outputParameters.z > 0.5 ? toneMapUncharted2(color) : color;
}

void main()
{
	int mode = int(u_graphicsDebug.x);
	vec3 color = vec3_splat(0.0);
	if (mode >= 13 && mode != 21)
	{
		if (u_graphicsDebug.w < 0.5) { gl_FragColor = vec4(color, 1.0); return; }
		vec2 uv = v_texcoord0;
		float aspect = u_debugSource.x / u_debugSource.y;
		vec2 fit = vec2(max(1.0, u_debugProbe.y / aspect), max(1.0, aspect / u_debugProbe.y));
		uv = (uv - 0.5) * fit + 0.5;
		if (any(lessThan(uv, vec2_splat(0.0))) || any(greaterThan(uv, vec2_splat(1.0))))
		{
			gl_FragColor = vec4(color, 1.0);
			return;
		}
		if (mode == 19)
		{
			float layer = min(floor(uv.y * 2.0), 1.0);
			float depth = u_shadowInfo[int(layer)].z > 0.5
				? texture2DArrayLod(s_shadowAtlas, vec3(uv.x, uv.y * 2.0 - layer, layer), 0.0).r : 0.0;
			color = toLinearAccurate(vec3_splat(saturate(depth)));
		}
		else if (mode == 20)
		{
			vec2 tile = min(floor(uv * vec2(15.0, 12.0)), vec2(14.0, 11.0));
			int index = int(tile.y) * 15 + int(tile.x), slot = index / 6, face = index % 6;
			float type = u_localShadowTypes[slot / 4][slot % 4];
			vec2 coordinate = uv * vec2(15.0, 12.0) - tile;
			if (type >= 0.0 && (type < 0.5 || face == 0))
			{
				float depth = type > 0.5 ? texture2DArrayLod(s_spotShadows, vec3(coordinate, float(slot)), 0.0).r
					: textureCubeArrayLod(s_pointShadows, vec4(environmentDirection(coordinate, float(face)), float(slot)), 0.0).r;
				color = toLinearAccurate(vec3_splat(saturate(depth)));
			}
		}
		else if (mode == 22)
		{
			float longitude = (uv.x * 2.0 - 1.0) * 3.14159265;
			float latitude = (0.5 - uv.y) * 3.14159265;
			vec3 direction = vec3(cos(latitude) * sin(longitude), sin(latitude), cos(latitude) * cos(longitude));
			color = debugHdr(textureCubeLod(s_graphicsDebugSky, direction, u_graphicsDebug.z).rgb * u_debugSky.rgb);
		}
		else
		{
			vec4 value = texture2DLod(s_graphicsDebug, uv * u_debugSource.zw, u_graphicsDebug.z);
			if (mode == 13) { color = debugHdr(ddgiDecodeIrradiance(value.rgb)); }
			else if (mode == 14) { color = toLinearAccurate(vec3_splat(saturate(value.r * u_debugProbe.x / u_graphicsDebug.y))); }
			else if (mode == 15)
			{
				float deviation = sqrt(max(0.0, value.g - value.r * value.r)) * u_debugProbe.x;
				color = toLinearAccurate(vec3_splat(saturate(deviation / u_graphicsDebug.y)));
			}
			else if (mode == 16 || mode == 17) { color = value.a >= 0.0 ? debugHdr(value.rgb) : vec3_splat(0.0); }
			else if (mode == 18) { color = toLinearAccurate(vec3_splat(saturate(length(value.xyz)))); }
			else { color = toLinearAccurate(vec3_splat(saturate(value.r))); }
		}
	}
	else
	{
		vec4 normal = texture2D(s_normal, v_texcoord0);
		if (normal.a < 0.5) { gl_FragColor = vec4(color, 1.0); return; }
		vec3 parameters = texture2D(s_material, v_texcoord0).rgb;
		if (mode == 1) { color = texture2D(s_albedo, v_texcoord0).rgb; }
		else if (mode == 2) { color = toLinearAccurate(decodeNormalUint(normal.xyz) * 0.5 + 0.5); }
		else if (mode == 3)
		{
			vec3 geometric = reflectionDecodeNormal(texture2D(s_reflectionSurface, v_texcoord0).zw);
			color = toLinearAccurate(geometric * 0.5 + 0.5);
		}
		else if (mode == 5) { color = toLinearAccurate(vec3_splat(parameters.r)); }
		else if (mode == 6) { color = toLinearAccurate(vec3_splat(texture2D(s_roughness, v_texcoord0).r)); }
		else if (mode == 7) { color = toLinearAccurate(vec3_splat(parameters.b)); }
		else if (mode == 8) { color = debugHdr(texture2D(s_emissive, v_texcoord0).rgb); }
		else
		{
			vec3 clip = vec3(v_texcoord0 * 2.0 - 1.0, toClipSpaceDepth(texture2D(s_depth, v_texcoord0).r));
			clip.y = toClipSpaceY(clip.y);
			vec3 position = clipToWorld(u_invSceneViewProj, clip);
			if (mode == 4)
			{
				float depth = dot(position - u_sceneCamera.xyz, u_sceneDirection.xyz);
				color = toLinearAccurate(vec3_splat(saturate(depth / u_graphicsDebug.y)));
			}
			else if (u_graphicsDebug.w > 0.5)
			{
				int light = int(u_shadowSelection.x);
				float depth = mul(u_shadowView[light], vec4(position, 1.0)).z;
				vec3 cascade = depth < u_shadowSplits[light].x ? vec3(1.0, 0.25, 0.25)
					: (depth < u_shadowSplits[light].y ? vec3(0.25, 1.0, 0.25)
					: (depth < u_shadowSplits[light].z ? vec3(0.25, 0.5, 1.0) : vec3(1.0, 0.75, 0.25)));
				color = depth <= u_shadowSplits[light].w ? toLinearAccurate(cascade) : vec3_splat(0.0);
			}
		}
	}
	gl_FragColor = vec4(saturate(color), 1.0);
}
