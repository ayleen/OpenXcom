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
// Phase 25 R3: tangent-space normal map (unit 4; LINEAR non-sRGB).
uniform sampler2D u_normalMap;
uniform vec3      u_sunDir;
uniform int       u_hasNormalMap;

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
// Bit mapping:  NW=bit3, NE=bit2, SE=bit1, SW=bit0.
// §22.0.2 calibration (derived from Camera::convertMapToScreen — screenX=16x-16y,
// screenY=8x+8y — and computeWangMask): grid-N renders screen upper-right, W
// upper-left, E lower-right, S lower-left, so the shared Wang corners land on the
// diamond VERTICES as  NW→top  NE→right  SE→bottom  SW→left.  toDiamond maps those
// vertices to d-corners  left→(0,0) top→(1,0) bottom→(0,1) right→(1,1), giving the
// bilinear placement below (the prior nw/ne/sw/se order was rotated 90°).
float cornerField(vec2 d, uint m)
{
    float nw = float((m >> 3u) & 1u);
    float ne = float((m >> 2u) & 1u);
    float se = float((m >> 1u) & 1u);
    float sw = float( m        & 1u);
    //  d(0,0)=left=SW   d(1,0)=top=NW   d(0,1)=bottom=SE   d(1,1)=right=NE
    return mix(mix(sw, nw, d.x), mix(se, ne, d.x), d.y);
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

    // §25 edge-localised blend. cornerField is 0.5 at a single-foreign tile's
    // CENTRE (bilinear of 1/0 corners), so smoothstep(0.5±fe, fld) smeared the mix
    // halfway across the tile and gave a bright core + wide halo that read as a
    // diamond. Remap so only the FOREIGN half (fld 0.5→1) blends: `edge` is 0
    // through the self interior and 1 at the shared seam — the tile keeps its own
    // colour at its centre and dissolves only toward the boundary.
    float edge = clamp((fld - 0.5) * 2.0, 0.0, 1.0);
    float w    = smoothstep(0.5 - fe, 0.5 + fe, edge + nz * v_noiseAmp);

    // Seam-symmetry: cap the mix at 0.5 so each tile reaches at most a 50/50 blend
    // at the seam (edge → 1 there). Both neighbours render an IDENTICAL 50/50
    // colour on the seam, so the result is independent of draw order — `baseline:
    // none` datasets (SAND) write no floor depth, so the blend pass is a pure
    // painter's-order draw, and before the cap the front tile's diamond occluded
    // the back tile's feather on one diagonal only. 0.5 is the unique cap that
    // makes the seam pixel agree from both sides (0.5·B+0.5·A == 0.5·A+0.5·B).
    vec4 c = mix(self, nbr, clamp(w, 0.0, 1.0) * 0.5);
    if (c.a < 0.01) discard;

    if (v_shade >= 15.5)
    {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float shadeF = texture(u_shadeCurve, vec2((v_shade + 0.5) / 16.0, 0.5)).r;

    // Phase 25 R3/R4: blend self+neighbour normals AND AO with the SAME w (and 0.5
    // cap) as the colour mix above so relief + occlusion stay continuous across the
    // seam, then Lambert: AO * (0.6 + 0.4 * max(N·L, 0)).
    float relief = 1.0;
    if (u_hasNormalMap == 1)
    {
        vec4 nmSelf = texture(u_normalMap, v_uv          + frameOff);
        vec4 nmNbr  = texture(u_normalMap, v_neighbourUV + frameOff);
        vec4 nm = mix(nmSelf, nmNbr, clamp(w, 0.0, 1.0) * 0.5);
        vec3  n  = normalize(nm.rgb * 2.0 - 1.0);
        float ao = nm.a;                                   // R4: ambient occlusion
        relief = ao * (0.6 + 0.4 * max(dot(n, normalize(u_sunDir)), 0.0));
    }
    fragColor = vec4(c.rgb * shadeF * relief, c.a);
}
