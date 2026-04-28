/* textured.frag — single-texture sampler (smoke-test shader, Phase 8b).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Uniforms:
 *   u_tex — sampler2D bound to texture unit 0
 * Inputs:
 *   v_uv  — interpolated UV from passthrough.vert
 */
uniform sampler2D u_tex;
in      vec2      v_uv;
out     vec4      out_color;

void main()
{
    out_color = texture(u_tex, v_uv);
}
