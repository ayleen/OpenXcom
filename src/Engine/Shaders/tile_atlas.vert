// tile_atlas.vert — Phase 11.3: per-instance tile rendering vertex shader.
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().
//
// Per-vertex data (6 vertices forming 2 triangles per tile):
//   a_corner       — corner position within tile: (0,0),(1,0),(0,1),(0,1),(1,0),(1,1)
//
// Per-instance data (divisor=1 via glVertexAttribDivisor):
//   a_screenPos    — top-left of tile in screen pixels
//   a_atlasUV      — UV of tile's primary frame top-left in atlas (0..1)
//   a_shade        — shade level 0..15
//   a_animFrameCount — total animation frames for this tile (>=1; 1 = static)
//   a_alphaMask    — MCD opacity flag (0 or 1)
//
// Uniforms:
//   u_screenSize   — logical viewport in pixels
//   u_tilePixelSize — tile size in pixels (e.g. 32x40 or 64x80)
//   u_tileUVSize   — one tile in UV space (tileW/atlasW, tileH/atlasH)

layout(location=0) in vec2  a_corner;
layout(location=1) in vec2  a_screenPos;
layout(location=2) in vec2  a_atlasUV;
layout(location=3) in float a_shade;
layout(location=4) in float a_animFrameCount;
layout(location=5) in float a_alphaMask;
layout(location=6) in float a_iso;       // iso priority [0..1]: larger = closer to camera

uniform vec2 u_screenSize;
uniform vec2 u_tilePixelSize;
uniform vec2 u_tileUVSize;

out vec2  v_uv;
out float v_shade;
out float v_animFrameCount;
out float v_alphaMask;

void main()
{
    // Phase 17.1: add 2.0px geometry overdraw (1.0px on each side) to close 
    // sub-pixel gaps. Stretch UVs slightly to match.
    vec2 overdraw = vec2(1.0);
    vec2 offset = (a_corner * 2.0 - 1.0) * overdraw;
    vec2 pixelPos = a_screenPos + a_corner * u_tilePixelSize + offset;

    // Convert to NDC [-1, +1].
    vec2 ndc = (pixelPos / u_screenSize) * 2.0 - 1.0;

    // Flip Y: SDL uses top-left origin, GL uses bottom-left.
    ndc.y = -ndc.y;

    // Iso priority → NDC z. Smaller z wins under glDepthFunc(LESS), so larger
    // iso (closer to camera) maps to smaller z and draws on top.
    float ndcZ = 1.0 - 2.0 * a_iso;
    gl_Position = vec4(ndc, ndcZ, 1.0);

    // Atlas UV: base UV offset by this corner's fraction of one tile.
    v_uv = a_atlasUV + a_corner * u_tileUVSize;

    // Pass per-instance data straight through to the fragment shader.
    v_shade          = a_shade;
    v_animFrameCount = a_animFrameCount;
    v_alphaMask      = a_alphaMask;
}
