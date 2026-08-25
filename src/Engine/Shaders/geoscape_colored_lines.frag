/* geoscape_colored_lines.frag -- Phase 46.4 Section 15 P0 radar/flight
 * one-draw batch.
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Emits the per-vertex colour unchanged; paired endpoints are identical, so
 * the fragment output equals the raster-step shade resolution result.
 */
in  vec4 v_color;
out vec4 out_color;

void main()
{
	out_color = v_color;
}
