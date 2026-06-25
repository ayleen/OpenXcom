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
// Calypso P30: optional per-edge isometric clip (used to cut a blood/scorch decal along the
// specific tile edge(s) where a door is, instead of masking the whole tile diamond — that
// kept the splat's natural shape on the other sides instead of a solid filled diamond).
// u_clipEdges = (W,N,E,S) flags; unset(0,0,0,0) = no clip, so every other caller is
// unaffected. Centre + half-extents are the tile floor diamond in window pixels (gl_FragCoord).
// Each active edge discards the half-plane beyond it (toward that neighbour tile): the four
// neighbour directions in window coords are W(-hx,+hy) N(+hx,+hy) E(+hx,-hy) S(-hx,-hy); the
// dividing line is the perpendicular bisector, i.e. dot(frag-centre, dir) > 0.5*|dir|^2.
uniform vec4      u_clipEdges;
uniform vec2      u_clipCenter;
uniform vec2      u_clipHalf;
in      vec2      v_uv;
out     vec4      out_color;

void main()
{
    if (any(greaterThan(u_clipEdges, vec4(0.0))))
    {
        vec2  f  = gl_FragCoord.xy - u_clipCenter;
        float r2 = 0.5 * (u_clipHalf.x * u_clipHalf.x + u_clipHalf.y * u_clipHalf.y);
        if (u_clipEdges.x > 0.0 && dot(f, vec2(-u_clipHalf.x,  u_clipHalf.y)) > r2) discard; // west
        if (u_clipEdges.y > 0.0 && dot(f, vec2( u_clipHalf.x,  u_clipHalf.y)) > r2) discard; // north
        if (u_clipEdges.z > 0.0 && dot(f, vec2( u_clipHalf.x, -u_clipHalf.y)) > r2) discard; // east
        if (u_clipEdges.w > 0.0 && dot(f, vec2(-u_clipHalf.x, -u_clipHalf.y)) > r2) discard; // south
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
