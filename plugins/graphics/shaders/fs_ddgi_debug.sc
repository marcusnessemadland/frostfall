$input v_probe

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#define FROST_DDGI_SAMPLE 1
#include "ddgi.sh"

SAMPLER2D(s_depth, 0);
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_ddgiDebug;

void main()
{
	vec2 uv = (gl_FragCoord.xy - u_viewRect.xy) / u_viewRect.zw;
	vec2 clip = uv * 2.0 - 1.0;
	clip.y = toClipSpaceY(clip.y);
	vec3 nearPoint = clipToWorld(u_invSceneViewProj, vec3(clip, toClipSpaceDepth(0.0)));
	vec3 farPoint = clipToWorld(u_invSceneViewProj, vec3(clip, toClipSpaceDepth(1.0)));
	vec3 origin = mix(u_sceneCamera.xyz, nearPoint, u_sceneCamera.w);
	vec3 direction = normalize(farPoint - origin);
	vec3 offset = origin - v_probe.xyz;
	float b = dot(offset, direction);
	float c = dot(offset, offset) - u_ddgiDebug.x * u_ddgiDebug.x;
	float discriminant = b * b - c;
	if (discriminant < 0.0) { discard; }
	float root = sqrt(max(discriminant, 0.0));
	float distance = -b - root;
	if (distance < 0.0) { distance = -b + root; }
	if (distance < 0.0) { discard; }
	vec3 position = origin + direction * distance;
	vec4 projected = mul(u_viewProj, vec4(position, 1.0));
	float depth = toDepthTextureZ(projected.z / projected.w);
	if (depth < 0.0 || depth > 1.0) { discard; }
	if (u_ddgiDebug.y > 0.5 && depth > texture2D(s_depth, uv).r + 0.000002) { discard; }
	vec3 normal = normalize(position - v_probe.xyz);
	vec3 color = vec3_splat(0.18);
	if (u_ddgiDebug.z > 0.5)
	{
		vec2 atlas = ddgiUv(ddgiTile(ddgiCell(uint(v_probe.w + 0.5))), normal, 6.0);
		vec3 encoded = texture2DLod(s_ddgiIrradiance, atlas, 0.0).rgb;
		color = ddgiDecodeIrradiance(encoded);
	}
	gl_FragDepth = depth;
	gl_FragColor = vec4(color, 1.0);
}
