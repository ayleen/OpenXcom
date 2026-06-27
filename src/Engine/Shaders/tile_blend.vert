// tile_blend.vert — Phase 22: per-instance blend between two surface cells.
//
// BlendInstance layout (Map.h, stride=64 bytes):
//   loc  0  a_corner          vec2  per-vertex
//   loc  1  a_screenPos       vec2  per-instance  offset  0
//   loc  2  a_selfUV          vec2  per-instance  offset  8
//   loc  3  a_neighbourUV     vec2  per-instance  offset 16
//   loc  4  a_worldPos        vec2  per-instance  offset 24
//   loc  5  a_wangMask        uint  per-instance  offset 32
//   loc  6  a_shade           float per-instance  offset 36
//   loc  7  a_alphaMask       float per-instance  offset 40
//   loc  8  a_iso             float per-instance  offset 44
//   loc  9  a_animFrameCount  float per-instance  offset 48
//   loc 10  a_feather         float per-instance  offset 52
//   loc 11  a_noiseScale      float per-instance  offset 56
//   loc 12  a_noiseAmp        float per-instance  offset 60
//
// toDiamond is affine, so compute once per vertex and let the rasterizer
// interpolate — no per-fragment cost and drops one varying (O1, §22.4).
//
// No #version / precision — Shader::compile() prepends the platform preamble.

layout(location=0)  in vec2  a_corner;
layout(location=1)  in vec2  a_screenPos;
layout(location=2)  in vec2  a_selfUV;
layout(location=3)  in vec2  a_neighbourUV;
layout(location=4)  in vec2  a_worldPos;
layout(location=5)  in uint  a_wangMask;
layout(location=6)  in float a_shade;
layout(location=7)  in float a_alphaMask;
layout(location=8)  in float a_iso;
layout(location=9)  in float a_animFrameCount;
layout(location=10) in float a_feather;
layout(location=11) in float a_noiseScale;
layout(location=12) in float a_noiseAmp;

uniform vec2 u_screenSize;
uniform vec2 u_tilePixelSize;
uniform vec2 u_tileUVSize;

out vec2  v_uv;
out vec2  v_neighbourUV;
out vec2  v_diamondUV;
out vec2  v_worldUV;
flat out uint v_wangMask;
out float v_shade;
out float v_animFrameCount;
out float v_alphaMask;
out float v_feather;
out float v_noiseScale;
out float v_noiseAmp;

// Diamond-local projection for the floor iso diamond (§22.0.2).
// The floor diamond occupies the lower band of the cell (y ∈ [0.6,1.0] in
// tile space).  Coefficients calibrate to the actual make_diamond_mask edges:
//   d = 2.5*(p.y − 0.8)  → the diagonal slope in iso
//   u = p.x − d           → diamond "left–right" axis
//   v = p.x + d           → diamond "right–left" axis
// Both axes range ≈ [−0.5, 1.5] across the quad; cornerField bilinear mixing
// works in this range as long as corner values are at the extremes (P13).
vec2 toDiamond(vec2 p)
{
    // §25 calibration: make_diamond_mask places the floor diamond at source rows
    // 24..39 of a 40-row tile → p.y ∈ [0.6, 0.975], widest at rows 31/32, i.e.
    // centred at 0.7875 with a half-height of 0.1875 — NOT 0.8 ± 0.2. The old
    // 2.5·(p.y−0.8) mapped the BOTTOM vertex to d≈0.44 instead of 0.5, so the
    // corner field was compressed toward the bottom and the seam feathered wider
    // at the top than the bottom (the "blends on top, hard on the bottom" skew).
    // slope 1/0.1875 = 8/3 about centre 0.7875 sends all four vertices to the
    // exact unit-square corners the cornerField expects.
    float d = (8.0 / 3.0) * (p.y - 0.7875);
    return vec2(p.x - d, p.x + d);
}

void main()
{
    // Same 4px geometry overdraw as tile_atlas_rgba.vert to close sub-pixel gaps.
    vec2 overdraw = vec2(2.0);
    vec2 offset   = (a_corner * 2.0 - 1.0) * overdraw;
    vec2 pixelPos = a_screenPos + a_corner * u_tilePixelSize + offset;

    vec2 ndc = (pixelPos / u_screenSize) * 2.0 - 1.0;
    ndc.y    = -ndc.y;
    gl_Position = vec4(ndc, 1.0 - 2.0 * a_iso, 1.0);

    v_uv          = a_selfUV      + a_corner * u_tileUVSize;
    v_neighbourUV = a_neighbourUV + a_corner * u_tileUVSize;

    // Diamond-local coord for the corner field + world-anchored noise UV (P14).
    v_diamondUV = toDiamond(a_corner);
    v_worldUV   = a_worldPos + v_diamondUV;

    v_wangMask       = a_wangMask;
    v_shade          = a_shade;
    v_animFrameCount = a_animFrameCount;
    v_alphaMask      = a_alphaMask;
    v_feather        = a_feather;
    v_noiseScale     = a_noiseScale;
    v_noiseAmp       = a_noiseAmp;
}
