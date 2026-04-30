// tile_atlas.frag — Phase 11.5: exact shade-table lookup via GPU texture.
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().
//
// Inputs from vertex shader:
//   v_uv            — interpolated atlas UV for the tile corner
//   v_shade         — shade level 0..15 (per-instance)
//   v_animFrameCount — total animation frames for this tile (>=1)
//   v_alphaMask     — MCD opacity flag (0 = discard whole tile, 1 = render)
//
// Uniforms:
//   u_atlas         — R8 palette-index tile atlas; texel value = palette index / 255.
//                     Index 0 = transparent (discard).
//   u_shadeTable    — RGBA8 shade LUT, 16×256 (col = shade 0..15, row = palIdx 0..255).
//                     Both samplers must use GL_NEAREST (no palette index interpolation).
//   u_animFrame     — 0..1 fractional position in the animation cycle
//   u_tileUVSize    — one tile in atlas UV space (tileW/atlasW, tileH/atlasH)

uniform sampler2D u_atlas;
uniform sampler2D u_shadeTable;
uniform float     u_animFrame;
uniform vec2      u_tileUVSize;

in vec2  v_uv;
in float v_shade;
in float v_animFrameCount;
in float v_alphaMask;

out vec4 fragColor;

void main()
{
    // Discard tiles marked as fully transparent by the MCD opacity flag.
    if (v_alphaMask < 0.5) discard;

    // Resolve the current animation frame and offset the UV horizontally.
    float frame = floor(u_animFrame * v_animFrameCount);
    vec2 uv = v_uv + vec2(frame * u_tileUVSize.x, 0.0);

    // Sample atlas: R channel holds the palette index normalised to [0, 1].
    // Multiply by 255 and round to recover the integer index.
    float palNorm = texture(u_atlas, uv).r;

    // Palette index 0 is always transparent.
    if (palNorm < (0.5 / 255.0)) discard;

    // Shade-table lookup using texel-centre coordinates to avoid bleed
    // between adjacent entries (both atlas and shade table use GL_NEAREST,
    // but the explicit + 0.5 guard is cheap insurance).
    float shadeU = (v_shade + 0.5) / 16.0;
    float shadeV = (palNorm * 255.0 + 0.5) / 256.0;
    vec4 shaded = texture(u_shadeTable, vec2(shadeU, shadeV));

    fragColor = vec4(shaded.rgb, shaded.a);
}
