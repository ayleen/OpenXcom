/* textured.frag — single-texture sampler (Phase 8b; u_darken added Block 11.8).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Uniforms:
 *   u_tex    — sampler2D bound to texture unit 0
 *   u_darken — darkening factor: 0.0 = normal (GLSL default), 1.0 = full black
 * Inputs:
 *   v_uv  — interpolated UV from passthrough.vert
 */
uniform sampler2D u_tex;
uniform float     u_darken;
in      vec2      v_uv;
out     vec4      out_color;

void main()
{
    vec4 c = texture(u_tex, v_uv);
    out_color = vec4(c.rgb * (1.0 - u_darken), c.a);
}
