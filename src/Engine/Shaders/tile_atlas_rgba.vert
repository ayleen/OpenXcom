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
// Unit GraphSubset shared by RGBA colour and depth replays; zw<=0 keeps
// legacy terrain/full-quad instances unchanged.
layout(location=7) in vec4  a_clipRect;

uniform vec2 u_screenSize;
uniform vec2 u_tilePixelSize;
uniform vec2 u_tileUVSize;
// Unit RGBA frames must use the exact same 1px-per-side expansion as their R8
// fallback/mask. Terrain RGBA keeps its established 2px expansion.
uniform highp int u_unitGeometry;

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
    bool hasClip = a_clipRect.z > 0.0 && a_clipRect.w > 0.0;
    vec2 clipOffset = hasClip ? a_clipRect.xy : vec2(0.0);
    vec2 clipScale  = hasClip ? a_clipRect.zw : vec2(1.0);
    vec2 clippedCorner = clipOffset + a_corner * clipScale;
    bool partialClip = hasClip && (clipOffset.x > 0.0 || clipOffset.y > 0.0
        || clipScale.x < 1.0 || clipScale.y < 1.0);
    vec2 overdraw = vec2(partialClip ? 0.0 : (u_unitGeometry == 1 ? 1.0 : 2.0));
    vec2 offset = (a_corner * 2.0 - 1.0) * overdraw;
    vec2 pixelPos = a_screenPos + clippedCorner * u_tilePixelSize + offset;

    vec2 ndc = (pixelPos / u_screenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float ndcZ = 1.0 - 2.0 * a_iso;
    gl_Position = vec4(ndc, ndcZ, 1.0);

    v_uv = a_atlasUV + clippedCorner * u_tileUVSize;
    v_localUV        = clippedCorner;
    v_shade          = a_shade;
    v_animFrameCount = a_animFrameCount;
    v_alphaMask      = a_alphaMask;
}
