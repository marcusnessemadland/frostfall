/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

SAMPLERCUBE(s_environmentSpecular, 5);
SAMPLERCUBE(s_environmentDiffuse, 6);
SAMPLER2D(s_environmentBrdf, 7);
uniform vec4 u_environment; // intensity, maximum specular mip, unused, unused.

void environmentLighting(vec3 _normal, vec3 _view, vec3 _baseColor, float _metallic,
	float _roughness, float _occlusion, out vec3 _diffuse, out vec3 _specular)
{
	float NoV = max(dot(_normal, _view), 0.0001);
	vec3 f0 = mix(vec3_splat(0.04), _baseColor, _metallic);
	vec3 fresnel = f0 + (max(vec3_splat(1.0 - _roughness), f0) - f0) * pow(1.0 - NoV, 5.0);
	_diffuse = textureCube(s_environmentDiffuse, _normal).rgb
		* _baseColor * (1.0 - _metallic) * (vec3_splat(1.0) - fresnel);
	vec3 reflection = reflect(-_view, _normal);
	vec3 radiance = textureCubeLod(s_environmentSpecular, reflection, _roughness * u_environment.y).rgb;
	vec2 brdf = texture2D(s_environmentBrdf, vec2(NoV, _roughness)).rg;
	_specular = radiance * (f0 * brdf.x + brdf.y);
	_diffuse *= _occlusion * u_environment.x;
	_specular *= _occlusion * u_environment.x;
}
