/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

// Canonical cube face coordinates; _uv.y increases toward the bottom of a face.
vec3 environmentDirection(vec2 _uv, float _face)
{
	vec2 p = _uv * 2.0 - 1.0;
	if (_face < 0.5) { return normalize(vec3(1.0, -p.y, -p.x)); }
	if (_face < 1.5) { return normalize(vec3(-1.0, -p.y, p.x)); }
	if (_face < 2.5) { return normalize(vec3(p.x, 1.0, p.y)); }
	if (_face < 3.5) { return normalize(vec3(p.x, -1.0, -p.y)); }
	if (_face < 4.5) { return normalize(vec3(p.x, -p.y, 1.0)); }
	return normalize(vec3(-p.x, -p.y, -1.0));
}
