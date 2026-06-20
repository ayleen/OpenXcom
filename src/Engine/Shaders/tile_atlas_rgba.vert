// tile_atlas_rgba.vert — Phase 12.3: shared vertex shader for RGBA atlas path.
//
// Identical to tile_atlas.vert — only the fragment stage differs between the
// palette and RGBA variants.  Kept as a separate file so embed-shaders.py can
// pair it with tile_atlas_rgba.frag without special-casing.
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().

layout(location=0) in vec2  a_corner;
layout(location=1) in vec2  a_screenPos;
layout(location=2) in vec2  a_atlasUV;
layout(location=3) in float a_shade;
layout(location=4) in float a_animFrameCount;
layout(location=5) in float a_alphaMask;
layout(location=6) in float a_iso;

uniform vec2 u_screenSize;
uniform vec2 u_tilePixelSize;
uniform vec2 u_tileUVSize;

out vec2  v_uv;
out vec2  v_localUV;
out float v_shade;
out float v_animFrameCount;
out float v_alphaMask;

void main()
{
    // Phase 17.1: add 4.0px geometry overdraw (2.0px on each side) to close 
    // sub-pixel gaps. We do NOT expand UVs; instead we slightly stretch the 
    // texture over the expanded quad. This prevents sampling neighbor cells 
    // in the unguttered atlas while ensuring adjacent quads overlap.
    vec2 overdraw = vec2(2.0);
    vec2 offset = (a_corner * 2.0 - 1.0) * overdraw;
    vec2 pixelPos = a_screenPos + a_corner * u_tilePixelSize + offset;

    vec2 ndc = (pixelPos / u_screenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float ndcZ = 1.0 - 2.0 * a_iso;
    gl_Position = vec4(ndc, ndcZ, 1.0);

    v_uv = a_atlasUV + a_corner * u_tileUVSize;
    v_localUV        = a_corner;
    v_shade          = a_shade;
    v_animFrameCount = a_animFrameCount;
    v_alphaMask      = a_alphaMask;
}
