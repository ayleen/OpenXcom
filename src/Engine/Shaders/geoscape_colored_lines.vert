/* geoscape_colored_lines.vert -- Phase 46.4 Section 15 P0 radar/flight
 * one-draw batch.
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Attribute layout mirrors CalypsoGeoscapeColoredLineVertex exactly:
 *   a_pos (location 0) -- vec2 clip-space position
 *   a_col (location 1) -- vec4 normalised unsigned-byte RGBA
 * Both vertices generated from one raster-step command carry the identical
 * resolved colour, so no unintended interpolation happens across a segment.
 */
layout(location=0) in vec2 a_pos;
layout(location=1) in vec4 a_col;
out vec4 v_color;

void main()
{
	gl_Position = vec4(a_pos, 0.0, 1.0);
	v_color = a_col;
}
