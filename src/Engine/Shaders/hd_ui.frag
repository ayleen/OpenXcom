/* hd_ui.frag — HD UI overlay sampler (Phase 46.2-HD).
 *
 * Do NOT include #version or precision qualifiers here.
 * The platform preamble is prepended by Shader::compile().
 *
 * Samples one HD source texture (re-rasterised TTF text, an original RGBA
 * asset, or a procedural painter target) and applies an optional RGBA multiply.
 * The source alpha is straight (TTF_RenderUTF8_Blended / RGBA assets); the HD
 * UI stage sets a standard src-alpha / one-minus-src-alpha blend.
 *
 * Uniforms:
 *   u_tex   — sampler2D bound to texture unit 0
 *   u_color — RGBA multiply. Unset (all-zero) means "no modulation" → white,
 *             matching the shared textured.frag convention, so a plain
 *             untinted blit needs no uniform set. A tinted caller (e.g. a
 *             colour-keyed glyph) passes a non-zero colour.
 * Inputs:
 *   v_uv    — interpolated UV from hd_ui.vert
 */
uniform sampler2D u_tex;
uniform vec4      u_color;
uniform float     u_opacity; // Phase 46.4-F33 opening motion (1 = opaque)
in      vec2      v_uv;
out     vec4      out_color;

void main()
{
    vec4 c = texture(u_tex, v_uv);
    // A scalar bool: true iff every component is exactly 0 (the unset-uniform
    // sentinel). Treat that as opaque white so untinted callers are unaffected.
    vec4 mul = (u_color == vec4(0.0)) ? vec4(1.0) : u_color;
    out_color = vec4((c * mul).rgb, (c * mul).a * u_opacity);
}
