/* hd_ui.vert — HD UI overlay textured-quad vertex shader (Phase 46.2-HD).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * The HD UI overlay maps logical widget rectangles to physical device pixels
 * on the CPU (CalypsoHdUiModel), then converts the physical rectangle to
 * clip-space here-consumed positions, so this shader is a plain passthrough.
 *
 * Attribute layout (matches passthrough.vert / the Cursor pass):
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
