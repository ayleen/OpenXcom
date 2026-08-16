/* hd_ui_panel.frag — HD UI styled-panel SDF painter (Phase 46.2-HD styling).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Paints one rounded-rect panel from a signed-distance field: AA shape edge,
 * an N-px border ring, a two-stop directional gradient fill, and a soft
 * quadratic outer glow (drop shadows / accent halos). Replaces the
 * tinted-white-quad panel for items carrying CalypsoHdPanelStyle; shares the
 * hd_ui.vert passthrough (clip-space pos + quad UV).
 *
 * Uniforms (all sizes in physical device px, set by drawStyledPanel):
 *   u_quadSize    — full quad size (shape + glow padding)
 *   u_shapeOffset — shape top-left within the quad
 *   u_size        — shape size (the SDF box)
 *   u_radius      — corner radius in px
 *   u_borderWidth — border ring thickness in px (0 = none)
 *   u_borderColor — RGBA border colour
 *   u_fillTop/u_fillBottom — gradient stops (direction: u_gradDir)
 *   u_gradDir     — gradient direction (need not be normalized; clamped t)
 *   u_glowColor   — RGBA glow; alpha is the peak strength
 *   u_glowRadius  — outer falloff distance in px (0 = no glow)
 * Inputs:
 *   v_uv          — interpolated UV from hd_ui.vert (0..1 over the quad)
 */
uniform vec2  u_quadSize;
uniform vec2  u_shapeOffset;
uniform vec2  u_size;
uniform float u_radius;
uniform float u_borderWidth;
uniform vec4  u_borderColor;
uniform vec4  u_fillTop;
uniform vec4  u_fillBottom;
uniform vec2  u_gradDir;
uniform vec4  u_glowColor;
uniform float u_glowRadius;
uniform float u_opacity; // Phase 46.4-F33 opening motion (1 = opaque)
in  vec2 v_uv;
out vec4 out_color;

float sdRoundBox(vec2 p, vec2 b, float r)
{
	vec2 q = abs(p) - b + r;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
	vec2 half_ = u_size * 0.5;
	vec2 p = v_uv * u_quadSize - u_shapeOffset - half_;
	float d = sdRoundBox(p, half_, u_radius);
	float aa = max(fwidth(d), 1e-4);

	// shapeMask: 1 inside / 0 outside with an AA edge; coreMask: everything
	// deeper than the border ring; their difference is the ring itself.
	float shapeMask  = 1.0 - smoothstep(-aa, aa, d);
	float coreMask   = 1.0 - smoothstep(-u_borderWidth - aa, -u_borderWidth + aa, d);
	float borderMask = clamp(shapeMask - coreMask, 0.0, 1.0);

	// Gradient parameter: projection of the fragment onto the direction,
	// normalized by the shape's extent along that direction.
	float t = clamp(dot(p, u_gradDir) / max(dot(u_size, abs(u_gradDir)), 1.0) + 0.5, 0.0, 1.0);
	vec4 fill = mix(u_fillTop, u_fillBottom, t);
	vec4 shapeCol = mix(fill, u_borderColor, borderMask);
	float shapeA = shapeCol.a * shapeMask;

	// Soft outer glow: quadratic falloff over u_glowRadius beyond the edge,
	// suppressed under the shape itself.
	// F33-PARITY-003: monotonic OUTWARD falloff (1 at the edge, 0 at radius) --
	// the same formula as calypsoHdGlowFalloff in CalypsoHdSdfMath.h, mirrored
	// here so the pure native test and the GLSL can never disagree.
	float glowA = 0.0;
	if (u_glowRadius > 0.0)
	{
		float g = clamp(1.0 - d / u_glowRadius, 0.0, 1.0);
		glowA = u_glowColor.a * g * g * (1.0 - shapeMask);
	}

	vec3 rgb = mix(u_glowColor.rgb, shapeCol.rgb, shapeMask);
	out_color = vec4(rgb, max(shapeA, glowA) * u_opacity);
}
