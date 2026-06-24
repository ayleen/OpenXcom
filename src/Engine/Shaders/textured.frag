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
uniform vec3      u_tintEdge; // Calypso: radial-gradient outer colour (inner = u_tint)
uniform float     u_radial;   // Calypso: 0 = flat tint (unset default); >0 = radial mix amount
// Calypso P30: optional isometric-diamond clip (used to confine a blood/scorch decal to its
// own tile's floor footprint when an adjacent door opens). u_clipDiamond unset(0) = off, so
// every other caller is unaffected. Centre + half-extents are in window pixels (gl_FragCoord).
uniform float     u_clipDiamond;
uniform vec2      u_clipCenter;
uniform vec2      u_clipHalf;
in      vec2      v_uv;
out     vec4      out_color;

void main()
{
    if (u_clipDiamond > 0.0)
    {
        vec2 dd = abs(gl_FragCoord.xy - u_clipCenter) / max(u_clipHalf, vec2(1.0));
        if (dd.x + dd.y > 1.0) discard;   // outside the tile's floor diamond
    }
    vec4 c = texture(u_tex, v_uv + u_uvScroll);
    // Unset uniform (0,0,0) means "no tint" → white, so untinted callers are
    // unaffected; tinted callers pass a non-zero colour.
    vec3 tint = all(equal(u_tint, vec3(0.0))) ? vec3(1.0) : u_tint;
    // Calypso: radial colour ramp (u_tint at the centre → u_tintEdge at the rim).
    // u_radial unset (0) leaves every existing caller on the flat-tint path above.
    if (u_radial > 0.0)
    {
        float d = clamp(length(v_uv - vec2(0.5)) * 2.0 * u_radial, 0.0, 1.0);
        tint = mix(u_tint, u_tintEdge, smoothstep(0.0, 1.0, d));
    }
    float am = (u_alpha <= 0.0) ? 1.0 : u_alpha;   // unset → opaque
    out_color = vec4(c.rgb * tint * (1.0 - u_darken), c.a * am);
}
