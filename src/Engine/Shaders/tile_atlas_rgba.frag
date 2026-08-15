// tile_atlas_rgba.frag — Phase 12.3: RGBA atlas path, no shade-table lookup.
//
// Samples the atlas as RGBA8 (uploaded with GL_LINEAR filter so 256×320 source
// tiles downsample smoothly).  Phase 22 (P5): shade darkening now uses the same
// u_shadeCurve LUT as tile_blend.frag so HD overlay and blend tiles match at
// night (no seam at shade > 8).  u_shadeCurve is a 16×1 R8 texture built from
// the real palette shade table in Map::setPalette.  If u_shadeCurve is not
// bound the sampler returns 0.0 and the tile renders black — the shader relies
// on Map::drawTileGLPass binding it at unit 3 for every RGBA draw call.
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().

uniform sampler2D u_atlas;
uniform sampler2D u_shadeCurve;  // unit 3: 16×1 night ramp (Phase 22, P5)
uniform float     u_animFrame;
uniform vec2      u_tileUVSize;
uniform highp int u_unitGeometry;
// Phase 25 R3: tangent-space normal map (unit 4; LINEAR non-sRGB). u_hasNormalMap
// is reset per draw group in Map::drawTileGLPass so non-mapped datasets stay flat.
uniform sampler2D u_normalMap;
uniform vec3      u_sunDir;        // normalised in-shader
uniform int       u_hasNormalMap;  // 0 = skip relief; 1 = apply
// Phase 25 R6: material emissive (unit 5). RGB = glow colour, A = intensity.
// Added AFTER shade/relief so the tile self-lights in shadow; lands in the HDR
// SSAA buffer (R0) where the >1.0 highlights bloom. u_hasEmissive reset per group.
uniform sampler2D u_emissive;
uniform int       u_hasEmissive;       // 0 = none; 1 = add glow
uniform float     u_emissiveStrength;  // global live-tunable multiplier

in vec2  v_uv;
in vec2  v_localUV;
in float v_shade;
in float v_animFrameCount;
in float v_alphaMask;

out vec4 fragColor;

void main()
{
    if (v_alphaMask < 0.5) discard;

    float frame = floor(u_animFrame * v_animFrameCount);
    vec2 uv = v_uv + vec2(frame * u_tileUVSize.x, 0.0);

    vec4 c = texture(u_atlas, uv);

    // A value above 1.0 in alphaMask marks a floor-like O_OBJECT layer. Its
    // source still has the ordinary diamond mask, but the material fades out
    // well inside that diamond so it dissolves into the O_FLOOR below instead
    // of painting a visible, tile-sized rhombus.
    if (v_alphaMask > 1.5)
    {
        // Map the unit quad to the 2:1 isometric diamond. Each component is
        // 0..1 inside it; edge is the distance to the nearest diamond side.
        float d = 2.5 * (v_localUV.y - 0.8);
        vec2 diamondUV = vec2(v_localUV.x - d, v_localUV.x + d);
        float edge = min(min(diamondUV.x, diamondUV.y),
                         min(1.0 - diamondUV.x, 1.0 - diamondUV.y));
        c.a *= smoothstep(0.10, 0.35, edge);
    }
    // Discard only truly-transparent texels (covers the geometry-overdraw
    // border around each cell and clamp-to-edge samples outside the mask).
    if (c.a < 0.01) discard;

    // Undiscovered terrain remains opaque black. Visible units can also carry
    // shade 16 in dark Battlescape cells; keep their authored fill instead of
    // replacing the entire RGBA sprite with a featureless silhouette.
    if (v_shade >= 15.5 && u_unitGeometry != 1)
    {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Phase 22 (P5): luminance-ramp darkening from the palette shade table so
    // HD overlay tiles match the brightness of adjacent blend tiles at night.
    float shadeF  = texture(u_shadeCurve, vec2((v_shade + 0.5) / 16.0, 0.5)).r;
    vec3 authored = c.rgb;
    if (u_unitGeometry == 1)
    {
        float unitAmount = 1.0;
        float ambientFloor = mix(0.0, 0.90, unitAmount);
        shadeF = max(shadeF, ambientFloor);
        // Lift dark authored navy into a visible midtone while preserving hue
        // and highlights. The underwater grade still supplies the scene tint.
        authored = mix(authored, pow(authored, vec3(0.45)), unitAmount);
    }

    // Phase 25 R3/R4: optional normal-map diffuse relief + ambient occlusion.
    // RGB decodes the tangent-space normal (0.5 + N*0.5) → Lambert against the sun;
    // A is AO (crevice darkening). relief = AO * (0.6 ambient + 0.4 * max(N·L, 0)).
    // u_hasNormalMap==0 → identity.
    float relief = 1.0;
    if (u_hasNormalMap == 1)
    {
        vec4  nm = texture(u_normalMap, uv);
        vec3  n  = normalize(nm.rgb * 2.0 - 1.0);
        float ao = nm.a;                                   // R4: ambient occlusion
        relief = ao * (0.6 + 0.4 * max(dot(n, normalize(u_sunDir)), 0.0));
    }
    vec3 lit = authored * shadeF * relief;

    // Phase 25 R6: add material emission on top of the lit colour (self-lit, so it
    // survives night/shadow). Premultiplied by its own alpha; the global strength
    // can push it past 1.0 so the HDR tonemap (R0) blooms lava/bioluminescence.
    if (u_hasEmissive == 1)
    {
        vec4 em = texture(u_emissive, uv);
        lit += em.rgb * em.a * u_emissiveStrength;
    }
    fragColor = vec4(lit, c.a);
}
