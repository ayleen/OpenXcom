/* textured.frag — single-texture sampler (Phase 8b; u_darken added Block 11.8).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Uniforms:
 *   u_tex    — sampler2D bound to texture unit 0
 *   u_darken — darkening factor: 0.0 = normal (GLSL default), 1.0 = full black
 *   u_tint   — multiply tint (Phase 24). MUST be set to vec3(1) for untinted
 *              sprites — a GLSL uniform defaults to 0, which would blacken them.
 *              White-silhouette UI markers set this to the per-state colour.
 * Inputs:
 *   v_uv  — interpolated UV from passthrough.vert
 */
uniform sampler2D u_tex;
uniform float     u_darken;
uniform vec3      u_tint;
in      vec2      v_uv;
out     vec4      out_color;

void main()
{
    vec4 c = texture(u_tex, v_uv);
    out_color = vec4(c.rgb * u_tint * (1.0 - u_darken), c.a);
}
