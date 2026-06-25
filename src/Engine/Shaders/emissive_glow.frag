/* emissive_glow.frag — Phase 25 (R1): additive coloured emissive halo.
 *
 * Soft radial falloff tinted by u_tint and scaled by u_intensity (which may
 * exceed 1.0 so the HDR grade's bloom threshold lifts it into a glow). The pass
 * is drawn with glBlendFunc(GL_ONE, GL_ONE) → rgb is ADDED to the scene; the
 * alpha channel is colour-masked off by the caller, so out_color.a is ignored.
 *
 * Do NOT include #version or precision — Shader::compile() prepends the preamble.
 * Uses emissive_glow.vert (a_corner @0 → v_local in [-1,1]).
 */
uniform vec3  u_tint;
uniform float u_intensity;

in  vec2 v_local;      // -1..1 across the quad
out vec4 out_color;

void main()
{
    float d = length(v_local);
    // Bright soft core (gaussian-ish) with a smooth edge cut at the quad border
    // so the square geometry never shows.
    float core = exp(-d * d * 2.2);
    float edge = smoothstep(1.0, 0.0, d);
    float a = core * edge;
    out_color = vec4(u_tint * (a * u_intensity), 1.0);
}
