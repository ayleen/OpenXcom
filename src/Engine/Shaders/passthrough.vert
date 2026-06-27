/* passthrough.vert — fullscreen-quad vertex shader (Phase 8b).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Attribute layout:
 *   a_pos (location 0) — 2D clip-space position (-1..1)
 *   a_uv  (location 1) — normalised UV (0..1)
 */
layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;
out vec2 v_uv;

void main()
{
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
