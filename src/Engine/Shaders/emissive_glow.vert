/* emissive_glow.vert — Phase 25 (R1): per-source emissive halo.
 *
 * Maps a unit quad [0,1]² around a source centre given in base-resolution
 * pixels, using the SAME screen→NDC convention as tile_atlas.vert (so the halo
 * lands exactly on the tile at any SSAA scale — NDC is viewport-independent).
 *
 * Do NOT include #version or precision — Shader::compile() prepends the preamble.
 */
layout(location=0) in vec2 a_corner;   // unit quad corner [0,1]

uniform vec2 u_screenSize;             // base resolution (px)
uniform vec2 u_centerPx;               // halo centre (base-res px)
uniform vec2 u_halfPx;                 // halo half-extent (base-res px)

out vec2 v_local;                      // -1..1 across the quad

void main()
{
    vec2 local = a_corner * 2.0 - 1.0;             // [-1,1]
    v_local = local;
    vec2 pixelPos = u_centerPx + local * u_halfPx;
    vec2 ndc = (pixelPos / u_screenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;                                // SDL top-left → GL bottom-left
    gl_Position = vec4(ndc, 0.0, 1.0);
}
