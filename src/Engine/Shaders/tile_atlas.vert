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

in vec2  a_corner;
in vec2  a_screenPos;
in vec2  a_atlasUV;
in float a_shade;
in float a_animFrameCount;
in float a_alphaMask;

uniform vec2 u_screenSize;
uniform vec2 u_tilePixelSize;
uniform vec2 u_tileUVSize;

out vec2  v_uv;
out float v_shade;
out float v_animFrameCount;
out float v_alphaMask;

void main()
{
    // Build pixel-space position of this corner within the tile.
    vec2 pixelPos = a_screenPos + a_corner * u_tilePixelSize;

    // Convert to NDC [-1, +1].
    vec2 ndc = (pixelPos / u_screenSize) * 2.0 - 1.0;

    // Flip Y: SDL uses top-left origin, GL uses bottom-left.
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, 0.0, 1.0);

    // Atlas UV: base UV offset by this corner's fraction of one tile.
    v_uv = a_atlasUV + a_corner * u_tileUVSize;

    // Pass per-instance data straight through to the fragment shader.
    v_shade          = a_shade;
    v_animFrameCount = a_animFrameCount;
    v_alphaMask      = a_alphaMask;
}
