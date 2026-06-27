/* colorquad.frag — solid-colour quad (smoke-test shader, Phase 8b).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Uniforms:
 *   u_color — RGBA colour to fill the quad with
 */
uniform vec4 u_color;
out     vec4 out_color;

void main()
{
    out_color = u_color;
}
