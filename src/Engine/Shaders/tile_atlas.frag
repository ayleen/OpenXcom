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
// Phase 42 E1: optional production RGBA alpha mask for an HD-backed unit
// emission. Where the RGBA frame has any coverage, discard the R8 baseline so
// fractional authored edges blend with the previously painted behind part,
// not with their own opaque vanilla fallback. Transparent RGBA texels retain
// the R8 fallback. Unit painter draws set this per emission; terrain resets it.
uniform sampler2D u_hdMask;
uniform int       u_hasHdMask;
uniform vec4      u_hdMaskUv; // xy=frame origin, zw=frame UV size
uniform float     u_animFrame;
uniform vec2      u_tileUVSize;
// Phase 25 R7: unit "fake lighting". 0 = off (tiles + floor items render byte-for-
// byte as before); > 0 = apply a sprite-local vertical AO/relief to unit bodies so
// they gain volume + a grounding shadow, matching the lit seabed. Set per draw call
// (NOT in the cached shader setup — _tileShader is shared by tiles AND units, so a
// cached value would leak across the interleaved tile/unit row draws).
uniform float     u_unitShade;

in vec2  v_uv;
in vec2  v_localUV;   // sprite-local 0..1; .y = 0 top (head) … 1 bottom (feet)
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

    if (u_hasHdMask == 1)
    {
        float hdAlpha = texture(u_hdMask,
            u_hdMaskUv.xy + v_localUV * u_hdMaskUv.zw).a;
        if (hdAlpha >= 0.01) discard;
    }

    // Sample atlas: R channel holds the palette index normalised to [0, 1].
    // Multiply by 255 and round to recover the integer index.
    float palNorm = texture(u_atlas, uv).r;

    // Palette index 0 is always transparent.
    if (palNorm < (0.5 / 255.0)) discard;

    // Undiscovered tiles (v_shade==16 from CPU side) render as opaque black.
    // ShadeTable::get returns _black (0xFF000000) for shade>=16; emulate that
    // exactly here so canopy silhouettes occlude interiors the player has not
    // discovered yet, matching painter's blit behaviour.
    if (v_shade >= 15.5)
    {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Shade-table lookup using texel-centre coordinates to avoid bleed
    // between adjacent entries (both atlas and shade table use GL_NEAREST,
    // but the explicit + 0.5 guard is cheap insurance).
    float shadeU = (v_shade + 0.5) / 16.0;
    float shadeV = (palNorm * 255.0 + 0.5) / 256.0;
    vec4 shaded = texture(u_shadeTable, vec2(shadeU, shadeV));

    // Phase 25 R7: fake unit lighting (off for tiles → identical output). Overhead
    // ambient + ground-contact occlusion: lift the upper body, sink the feet, on a
    // soft vertical ramp. Cheap volume with no RGBA atlas / baked-AO art; the knob
    // (u_unitShade, default 1.0) scales the whole effect toward identity.
    if (u_unitShade > 0.001)
    {
        float t    = clamp(v_localUV.y, 0.0, 1.0);                 // head 0 … feet 1
        float fake = mix(1.06, 0.82, smoothstep(0.0, 1.0, t));     // +6% head, -18% feet
        fake       = mix(1.0, fake, clamp(u_unitShade, 0.0, 1.0)); // knob-scaled
        shaded.rgb = clamp(shaded.rgb * fake, 0.0, 1.0);
    }

    fragColor = vec4(shaded.rgb, shaded.a);
}
