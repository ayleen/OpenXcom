// tile_blend.frag — Phase 22: per-fragment runtime surface blend.
//
// Blends self ↔ neighbour cell using a bilinear corner field (cornerField)
// driven by the 4-bit wangMask, perturbed with world-anchored tileable noise
// so blend boundaries feel organic and do not swim under camera pan (P14).
//
// Shade darkening via u_shadeCurve matches tile_atlas_rgba.frag exactly so
// there is no night seam between blend tiles and flat HD overlay tiles (P5).
//
// No #version / precision — Shader::compile() prepends the platform preamble.

uniform sampler2D u_atlas;       // unit 0: RGBA overlay atlas
uniform sampler2D u_noise;       // unit 2: tiling noise (GL_REPEAT)
uniform sampler2D u_shadeCurve;  // unit 3: 16×1 night ramp
uniform vec2  u_tileUVSize;
uniform float u_animFrame;

in vec2  v_uv;
in vec2  v_neighbourUV;
in vec2  v_diamondUV;
in vec2  v_worldUV;
flat in uint v_wangMask;
in float v_shade;
in float v_animFrameCount;
in float v_alphaMask;
in float v_feather;
in float v_noiseScale;
in float v_noiseAmp;

out vec4 fragColor;

// Bilinear corner field in diamond-local space.
// Returns 0 (self cell) → 1 (neighbour cell).
// Bit mapping:  NW=bit3, NE=bit2, SE=bit1, SW=bit0 (§22.0.2 calibration).
// A bit is set when that corner is "foreign" (dominated by the neighbour type).
float cornerField(vec2 d, uint m)
{
    float nw = float((m >> 3u) & 1u);
    float ne = float((m >> 2u) & 1u);
    float se = float((m >> 1u) & 1u);
    float sw = float( m        & 1u);
    return mix(mix(nw, ne, d.x), mix(sw, se, d.x), d.y);
}

void main()
{
    if (v_alphaMask < 0.5) discard;

    float frame    = floor(u_animFrame * v_animFrameCount);
    vec2  frameOff = vec2(frame * u_tileUVSize.x, 0.0);

    vec4 self = texture(u_atlas, v_uv          + frameOff);
    vec4 nbr  = texture(u_atlas, v_neighbourUV + frameOff);

    float fld = cornerField(v_diamondUV, v_wangMask);
    // Noise sampled at world-tile coord so it stays still under camera pan (P14).
    float nz  = texture(u_noise, v_worldUV * v_noiseScale).r - 0.5;
    // Clamp feather ≥ 0.001 to guard smoothstep UB when edge0 == edge1 (P15).
    float fe  = max(v_feather, 0.001);
    float w   = smoothstep(0.5 - fe, 0.5 + fe, fld + nz * v_noiseAmp);

    vec4 c = mix(self, nbr, clamp(w, 0.0, 1.0));
    if (c.a < 0.01) discard;

    if (v_shade >= 15.5)
    {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float shadeF = texture(u_shadeCurve, vec2((v_shade + 0.5) / 16.0, 0.5)).r;
    fragColor = vec4(c.rgb * shadeF, c.a);
}
