$input v_texcoord0

/*
 * Adapted from bgfx/examples/21-deferred and 36-sky.
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * Copyright 2017 Stanislav Pidhorskyi. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"
#include "shadow.sh"

SAMPLER2D(s_scene, 0);
SAMPLER2D(s_albedo, 1);
SAMPLER2D(s_normal, 2);
SAMPLER2D(s_depth, 3);
SAMPLER2D(s_material, 4);
SAMPLER2D(s_emissive, 6);
uniform vec4 u_outputParameters; // x: exposure multiplier, y: dither, z: debug view, w: inverse depth range.
uniform mat4 u_invSceneViewProj;

// Khronos PBR Neutral, adapted for bgfx. Copyright 2024 The Khronos Group, Inc.
// Apache-2.0: https://github.com/KhronosGroup/ToneMapping/blob/main/LICENSE.md
vec3 toneMapNeutral(vec3 _color)
{
	float lowest = min(_color.r, min(_color.g, _color.b));
	float black = lowest < 0.08 ? lowest * (1.0 - 6.25 * lowest) : 0.04;
	_color -= black;
	float brightest = max(_color.r, max(_color.g, _color.b));
	if (brightest < 0.76)
	{
		return _color;
	}
	float compressed = 1.0 - 0.0576 / (brightest - 0.52);
	_color *= compressed / brightest;
	float saturation = 1.0 / (1.0 + 0.15 * (brightest - compressed));
	return mix(vec3_splat(compressed), _color, saturation);
}

vec3 reconstructWorldPosition(vec2 _uv, float _depth)
{
	vec3 clip = vec3(_uv * 2.0 - 1.0, toClipSpaceDepth(_depth));
	clip.y = toClipSpaceY(clip.y);
	return clipToWorld(u_invSceneViewProj, clip);
}

void main()
{
	vec3 scene = max(texture2D(s_scene, v_texcoord0).rgb, vec3_splat(0.0));
	float mode = u_outputParameters.z;
	if (mode < 0.5 || mode > 17.5)
	{
		vec3 color = toneMapNeutral(scene * u_outputParameters.x);
		// Dither in display space, then let the sRGB backbuffer perform final encoding.
		float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
		color = toGammaAccurate(color) + vec3_splat((noise - 0.5) * u_outputParameters.y / 255.0);
		gl_FragColor = vec4(toLinearAccurate(saturate(color)), 1.0);
		return;
	}

	if (mode > 12.5)
	{
		float depth = 1.0;
		if (u_shadowInfo.z > 0.5)
		{
			depth = mode > 16.5 ? texture2D(s_shadowAtlas, v_texcoord0).r
				: shadowDepth(v_texcoord0, int(mode - 13.0));
		}
		gl_FragColor = vec4(toLinearAccurate(vec3_splat(depth)), 1.0);
		return;
	}

	vec4 normal = texture2D(s_normal, v_texcoord0);
	float depth = texture2D(s_depth, v_texcoord0).x;
	vec3 color = vec3_splat(0.0);
	if (mode > 5.5 && mode < 6.5)
	{
		float luminance = max(dot(scene, vec3(0.2126, 0.7152, 0.0722)), 0.000001);
		float stops = saturate((log2(luminance / 0.18) + 8.0) / 16.0);
		color = saturate(vec3_splat(1.5) - abs(vec3_splat(stops * 4.0) - vec3(3.0, 2.0, 1.0)));
	}
	else if (mode > 2.5 && mode < 3.5)
	{
		color = vec3_splat(depth);
	}
	else if (normal.w > 0.5)
	{
		if (mode > 10.5)
		{
			vec3 position = reconstructWorldPosition(v_texcoord0, depth);
			vec3 worldNormal = normalize(decodeNormalUint(normal.xyz));
			float viewDepth = mul(u_sceneView, vec4(position, 1.0)).z;
			if (mode < 11.5 && u_shadowInfo.z > 0.5 && viewDepth < u_shadowInfo.y)
			{
				int cascade = shadowCascade(viewDepth);
				color = cascade == 0 ? vec3(1.0, 0.2, 0.2) : (cascade == 1 ? vec3(0.2, 1.0, 0.2)
					: (cascade == 2 ? vec3(0.2, 0.4, 1.0) : vec3(1.0, 1.0, 0.2)));
				color *= 0.25 + 0.75 * shadowVisibility(position, worldNormal);
			}
			else if (mode > 11.5) { color = vec3_splat(shadowVisibility(position, worldNormal)); }
			gl_FragColor = vec4(toLinearAccurate(color), 1.0);
			return;
		}
		if (mode > 6.5)
		{
			vec3 parameters = texture2D(s_material, v_texcoord0).rgb;
			if (mode < 7.5) { color = vec3_splat(parameters.r); }
			else if (mode < 8.5) { color = vec3_splat(parameters.g); }
			else if (mode < 9.5) { color = vec3_splat(parameters.b); }
			else
			{
				gl_FragColor = vec4(toneMapNeutral(texture2D(s_emissive, v_texcoord0).rgb), 1.0);
				return;
			}
			gl_FragColor = vec4(toLinearAccurate(color), 1.0);
			return;
		}
		if (mode < 1.5)
		{
			gl_FragColor = vec4(texture2D(s_albedo, v_texcoord0).rgb, 1.0);
			return;
		}
		if (mode < 2.5)
		{
			color = normalize(decodeNormalUint(normal.xyz)) * 0.5 + 0.5;
		}
		else
		{
			vec3 position = reconstructWorldPosition(v_texcoord0, depth);
			if (mode < 4.5)
			{
				float viewDepth = abs(mul(u_sceneView, vec4(position, 1.0)).z);
				color = vec3_splat(saturate(viewDepth * u_outputParameters.w));
			}
			else
			{
				color = fract(position * 0.1); // Repeating ten-unit XYZ bands.
			}
		}
	}
	// Data visualizations specify display colors and bypass scene exposure/tone mapping.
	gl_FragColor = vec4(toLinearAccurate(color), 1.0);
}
