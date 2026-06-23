/* textured.frag — single-texture sampler (Phase 8b; u_darken added Block 11.8).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Uniforms:
 *   u_tex    — sampler2D bound to texture unit 0
 *   u_darken — darkening factor: 0.0 = normal (GLSL default), 1.0 = full black
 *   u_tint   — multiply tint (Phase 24). Defaults safely: an unset uniform is 0,
 *              and the shader treats vec3(0) as "no tint" (white), so the many
 *              shared callers (system Cursor, WarningMessage, GpuSmokeState, the
 *              sprite/projectile/smoke passes) need no change. UI markers set it
 *              to the per-state colour.
 * Inputs:
 *   v_uv  — interpolated UV from passthrough.vert
 */
uniform sampler2D u_tex;
uniform float     u_darken;
uniform vec3      u_tint;
uniform float     u_alpha;   // Calypso: overall alpha multiply; unset(0) => 1.0 (callers unaffected)
uniform vec2      u_uvScroll; // Calypso: UV offset for drifting layers (unset(0) => none)
in      vec2      v_uv;
out     vec4      out_color;

void main()
{
    vec4 c = texture(u_tex, v_uv + u_uvScroll);
    // Unset uniform (0,0,0) means "no tint" → white, so untinted callers are
    // unaffected; tinted callers pass a non-zero colour.
    vec3 tint = all(equal(u_tint, vec3(0.0))) ? vec3(1.0) : u_tint;
    float am = (u_alpha <= 0.0) ? 1.0 : u_alpha;   // unset → opaque
    out_color = vec4(c.rgb * tint * (1.0 - u_darken), c.a * am);
}
