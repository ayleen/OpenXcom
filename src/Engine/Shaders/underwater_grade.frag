/* underwater_grade.frag - full-screen underwater colour grade + beauty FX.
 *
 * The vanilla TFTD battlescape gets its dark blue-green mood from the depth
 * palette baked into every 8-bit sprite. Our HD RGBA tile overlays bypass that
 * palette and render "Miami-bright". This pass samples the finished scene
 * (rendered into the SSAA colour texture) and re-grades it toward deep water,
 * then layers animated beauty effects on top: caustics, refraction wobble,
 * bubbles rising from seabed vents + breathing aquanauts, and marine snow.
 *
 * Runs PRE-composite (before the CPU surface with HUD/cursor/menus is drawn),
 * so the UI is never tinted.
 *
 * Do NOT include #version or precision - Shader::compile() prepends the preamble.
 * Uses passthrough.vert (a_pos @0, a_uv @1 -> v_uv).
 */
uniform sampler2D u_scene;
uniform float     u_strength;
uniform float     u_time;
uniform vec2      u_res;
uniform float     u_caustics;
uniform float     u_refract;
uniform float     u_bubbles;
uniform float     u_snow;
uniform float     u_godray;           // light shafts from the surface
uniform float     u_bloom;            // glow on bright spots
uniform float     u_breath;           // slow global light "breathing" pulse
uniform float     u_chroma;           // subtle chromatic aberration at edges
uniform float     u_unitbub;          // aquanaut breathing-bubble amount (own knob)
uniform int       u_unitCount;        // # of visible aquanauts
uniform vec2      u_unitPos[12];      // their screen UV (v_uv space; y up)
uniform float     u_shock;            // E2: explosion shockwave-ring distortion amount
uniform int       u_swCount;          // # active shockwaves
uniform vec2      u_swCenter[4];      // their v_uv centres
uniform float     u_swAge[4];         // 0..1 ring expansion
in      vec2      v_uv;
out     vec4      out_color;

float hash21(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Layered animated ridged sine net -> caustic light pattern.
float caustics(vec2 uv, float t)
{
    vec2 p = uv * vec2(7.0, 5.0);
    float c = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        float fi = float(i);
        vec2 q = p + fi * 1.7;
        float a = sin(q.x * 2.1 + t * 0.9 + sin(q.y * 1.7 - t * 0.6));
        float b = sin(q.y * 1.9 - t * 0.7 + sin(q.x * 2.3 + t * 0.5));
        c += a * b;
        p *= 1.6;
    }
    c = pow(clamp(c / 3.0 * 0.5 + 0.5, 0.0, 1.0), 3.0);
    return c;
}

// Soft dots on a hashed grid, scrolling at vel (uv/sec) - used for marine snow.
float particleLayer(vec2 uv, float t, float grid, vec2 vel, float size,
                    float seed, float aspect)
{
    vec2 g = uv * grid + vel * t * grid;
    vec2 cell = floor(g);
    vec2 f = fract(g);
    float r1 = hash21(cell + seed);
    float r2 = hash21(cell + seed + 5.2);
    float br = hash21(cell + seed + 9.1);
    vec2 c = vec2(0.2 + 0.6 * r1, 0.2 + 0.6 * r2);
    vec2 d = (f - c) * vec2(aspect, 1.0);
    return smoothstep(size, 0.0, length(d)) * (0.35 + 0.65 * br);
}

// Bubbles from vents distributed over the seabed: each active cell is a vent - a
// hollow bubble rises a short distance from its base point, pops/fades, and the
// vent re-fires. Sparse (only ~density of cells vent). Caller masks by the scene
// so vents sit on the floor, not the black void.
float bubbleLayer(vec2 uv, float t, float grid, float speed, float size,
                  float seed, float aspect, float density)
{
    vec2 g = uv * grid;
    vec2 cell = floor(g);
    vec2 f = fract(g);
    if (hash21(cell + seed + 3.7) > density) return 0.0;    // only some cells vent
    float r1 = hash21(cell + seed);
    float r2 = hash21(cell + seed + 5.2);
    float rt = fract(t * speed * (0.7 + 0.6 * r2) + r1);    // rise progress (loops)
    vec2 base = vec2(0.20 + 0.60 * r1, 0.12);               // vent point in the cell
    vec2 c = base + vec2(0.05 * sin(t * 1.2 + r1 * 6.28) * rt, rt * 0.64);
    float sz = size * (0.6 + 0.3 * r2);                     // capped size variety
    vec2 d = (f - c) * vec2(aspect, 1.0);
    float dist = length(d) / sz;
    float er  = (dist - 0.80) / 0.16;
    float rim = exp(-er * er);                              // hollow ring
    vec2  hp  = (d / sz) - vec2(-0.30, 0.30);               // highlight
    float hi  = exp(-(length(hp) / 0.26) * (length(hp) / 0.26));
    float fade = smoothstep(0.0, 0.12, rt) * (1.0 - smoothstep(0.75, 1.0, rt));
    return (rim * 0.85 + hi * 0.9) * step(dist, 1.25) * fade;
}

// HD breathing bubbles rising from an aquanaut at P (uv, y up). Each bubble runs
// its OWN continuous lifecycle (born small -> inflate -> rise -> DEFLATE to
// nothing) on its own period + phase, so they are never in sync — they pop/deflate
// one at a time, never all at once. Varied size + a slow shape wobble.
float unitBubbles(vec2 uv, vec2 P, float seed, float aspect, float t)
{
    float acc = 0.0;
    for (int k = 0; k < 4; ++k)
    {
        float fk = float(k);
        float rk = fract(sin((seed + fk * 12.9) * 91.7) * 43758.5453);  // per-bubble
        float period = 3.0 + 2.5 * rk;                      // each bubble's own life (s)
        float life = fract(t / period + rk);                // 0..1, staggered -> desynced
        float base = 0.00225 + 0.00375 * fract(rk * 7.13 + 0.3);  // different sizes (half)
        // born small -> inflate -> deflate away near the end of ITS life
        float inflate = smoothstep(0.0, 0.10, life);
        float deflate = 1.0 - smoothstep(0.62, 1.0, life);
        float sz = max(base * inflate * deflate * (1.0 + 0.25 * life), 1e-4);
        float wob = 0.26 * sin(t * 2.0 + rk * 6.28 + fk);   // living shape wobble
        vec2  sq = vec2(1.0 + wob, 1.0 / (1.0 + wob));
        float sway = 0.006 * sin(t * 1.6 + fk + rk * 6.28);
        vec2  bc = P + vec2(sway, 0.010 + life * 0.085);    // rises over its life
        vec2  d  = (uv - bc) * vec2(aspect, 1.0) * sq;
        float dn = length(d) / sz;
        float er = (dn - 0.80) / 0.16;
        float rim = exp(-er * er);                          // hollow ring
        vec2  hp = (d / sz) - vec2(-0.30, 0.30);
        float hi = exp(-(length(hp) / 0.26) * (length(hp) / 0.26));
        acc += (rim * 0.85 + hi * 0.9) * step(dn, 1.25);
    }
    return acc;
}

// Light shafts from the surface: a few soft, slightly-tilted, swaying beams,
// strong at the top of the screen (where light enters) and fading downward.
float godrays(vec2 uv, float t)
{
    float r = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        float fi = float(i);
        float x0 = 0.18 + 0.22 * fi + 0.04 * sin(t * 0.3 + fi * 1.7);
        float w  = 0.05 + 0.018 * fi;
        float e  = (uv.x - x0 - 0.10 * (1.0 - uv.y)) / w;   // tilt with depth
        r += exp(-e * e);
    }
    return r * smoothstep(0.0, 0.55, uv.y);                 // top-lit, fade down
}

// Cheap single-pass bloom: golden-angle disk taps of the scene, keep the bright
// part above a threshold, average -> soft glow added back over the graded colour.
vec3 bloomGlow(vec2 uv, float radius, float aspect)
{
    vec3 acc = vec3(0.0);
    for (int i = 0; i < 12; ++i)
    {
        float a  = float(i) * 2.39996323;                  // golden angle
        float rr = sqrt((float(i) + 0.5) / 12.0) * radius;
        vec2  o  = vec2(cos(a) / aspect, sin(a)) * rr;
        acc += max(texture(u_scene, uv + o).rgb - 0.62, 0.0);
    }
    return acc / 12.0;
}

void main()
{
    float s = clamp(u_strength, 0.0, 1.0);
    float aspect = u_res.x / max(u_res.y, 1.0);
    float breath = 1.0 + u_breath * 0.08 * sin(u_time * 0.6);   // slow light pulse

    // 0. refraction: gently wobble the sample UV (looking through moving water)
    vec2 uv = v_uv;
    if (u_refract > 0.0)
    {
        float w = 0.0018 * u_refract;
        uv += vec2(sin(v_uv.y * 11.0 + u_time * 1.3),
                   cos(v_uv.x * 13.0 + u_time * 1.1)) * w;
    }
    // 0b. E2 shockwave: each blast warps the scene in an expanding ring (a hydraulic
    //     compression wave). Underwater only (s>0). Sample UV pushed outward at the front.
    if (u_shock > 0.0 && s > 0.0)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (i >= u_swCount) break;
            vec2  rel = v_uv - u_swCenter[i];
            float dd  = length(vec2(rel.x * aspect, rel.y));   // circular in screen space
            float rad = u_swAge[i] * 0.55;                      // ring expands outward
            float ring  = exp(-pow((dd - rad) / 0.05, 2.0));    // thin gaussian front
            float decay = 1.0 - u_swAge[i];                     // fades as it grows
            vec2  dir = dd > 1e-4 ? rel / dd : vec2(0.0);
            uv += dir * ring * decay * u_shock * 0.03;
        }
    }
    vec3 c;
    if (u_chroma > 0.0)
    {
        vec2 off = (v_uv - 0.5) * u_chroma * 0.010;        // edge-weighted RGB split
        c = vec3(texture(u_scene, uv + off).r,
                 texture(u_scene, uv).g,
                 texture(u_scene, uv - off).b);
    }
    else
    {
        c = texture(u_scene, uv).rgb;
    }
    float lum0 = dot(c, vec3(0.299, 0.587, 0.114));

    // 1. desaturate
    c = mix(c, vec3(lum0), 0.12 + 0.50 * s);

    // 2. colour absorption (warm dies first) + dim
    vec3  tint = vec3(0.88 - 0.46 * s, 0.96 - 0.10 * s, 1.02);
    float dim  = 0.94 - 0.34 * s;
    c *= tint * dim;

    // 3. depth fog - thicker toward the top (farther/deeper in iso)
    vec3  fogCol = vec3(16.0, 56.0, 74.0) / 255.0;
    float fogAmt = (0.18 + 0.52 * s) * (v_uv.y * 0.78 + 0.22);
    c = mix(c, fogCol, clamp(fogAmt, 0.0, 1.0));

    float surf = smoothstep(0.04, 0.30, lum0);   // "there is scene here" mask
    // additive in-water particulate (snow, bubbles) must sit UNDER the depth
    // darkening/fog so it doesn't read as bright white floating on top
    float waterLit = dim * (1.0 - 0.45 * clamp(fogAmt, 0.0, 1.0));

    // 4. caustics - bright light-net on lit surfaces
    if (u_caustics > 0.0)
    {
        float net = caustics(v_uv, u_time);
        c += net * surf * (0.45 + 0.55 * v_uv.y)
             * u_caustics * breath * vec3(0.22, 0.46, 0.52);   // shimmer with the pulse
    }

    // 5. marine snow - slow drifting motes (two parallax layers)
    if (u_snow > 0.0)
    {
        float sn  = particleLayer(v_uv, u_time, 26.0, vec2(0.010, 0.018), 0.045, 1.0, aspect);
        sn += 0.6 * particleLayer(v_uv, u_time, 40.0, vec2(-0.014, 0.026), 0.035, 7.0, aspect);
        c += sn * u_snow * waterLit * vec3(0.45, 0.56, 0.60);
    }

    // 6. bubbles - hollow, rising from vents distributed across the SEABED (the
    //    `surf` mask keeps them on the floor, not the bottom edge of the screen)
    // seabed vents (OFF by default - screen-anchored; re-enable once world-anchored)
    if (u_bubbles > 0.0)
    {
        float bz  = bubbleLayer(v_uv, u_time,  7.0, 0.10, 0.30, 21.0, aspect, 0.10);
        bz += 0.8 * bubbleLayer(v_uv, u_time, 11.0, 0.13, 0.22, 33.0, aspect, 0.09);
        c += bz * surf * u_bubbles * waterLit * vec3(0.45, 0.62, 0.72);
    }
    // small breathing puffs from each visible aquanaut (our HD replacement for the
    // vanilla sprite bubbles, which are suppressed on this build)
    if (u_unitbub > 0.0)
    {
        for (int i = 0; i < 12; ++i)
        {
            if (i >= u_unitCount) break;
            c += unitBubbles(v_uv, u_unitPos[i], float(i) * 0.137, aspect, u_time)
                 * u_unitbub * waterLit * vec3(0.42, 0.60, 0.70);
        }
    }

    // 7. god rays — light shafts from the surface (shimmer with the pulse)
    if (u_godray > 0.0)
        c += godrays(v_uv, u_time) * u_godray * breath * vec3(0.16, 0.32, 0.40);

    // 8. bloom — soft glow on bright spots
    if (u_bloom > 0.0)
        c += bloomGlow(uv, 0.014, aspect) * u_bloom * vec3(0.90, 0.97, 1.0);

    // 9. global light breathing (the surface moves -> light dims and brightens)
    c *= breath;

    // 10. vignette
    vec2  d   = v_uv - vec2(0.5, 0.54);
    float r   = length(vec2(d.x * 1.05, d.y * 1.25));
    float vig = (0.28 + 0.62 * s) * pow(clamp(r - 0.35, 0.0, 1.0), 2.0);
    c *= (1.0 - vig);

    out_color = vec4(c, 1.0);
}
