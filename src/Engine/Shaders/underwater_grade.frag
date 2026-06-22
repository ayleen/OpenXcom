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
uniform float     u_unitbub;          // aquanaut breathing-bubble amount (own knob)
uniform int       u_unitCount;        // # of visible aquanauts
uniform vec2      u_unitPos[12];      // their screen UV (v_uv space; y up)
uniform float     u_unitBreath[12];   // vanilla exhale progress 0..1 (<0 = not exhaling)
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

// HD breathing bubbles for an aquanaut at P (uv, y up), driven by the VANILLA
// exhale progress bp (0..1; <0 = not exhaling). Same cadence/animation as vanilla
// (random start, ~16-step rise) but rendered as pretty HD hollow bubbles: a small
// cluster is released, staggered, rises above the head and fades.
float unitBubbles(vec2 uv, vec2 P, float bp, float seed, float aspect, float t)
{
    if (bp < 0.0) return 0.0;
    float acc = 0.0;
    for (int k = 0; k < 3; ++k)
    {
        float fk = float(k);
        float lp = bp - fk * 0.22;                          // staggered release
        if (lp < 0.0) continue;
        float rk = fract(sin((seed + fk) * 91.7) * 43758.5453);  // per-bubble random
        float base = 0.0045 + 0.0075 * rk;                  // DIFFERENT sizes per bubble
        // size over life: grow a touch, then SHRINK toward nothing near the top
        // (so it dwindles away instead of fading out)
        float sz = base * (1.0 + 0.30 * lp) * (1.0 - smoothstep(0.55, 1.0, lp) * 0.92);
        sz = max(sz, 1e-4);
        // shape: slow area-preserving wobble -> non-round, living bubble form
        float wob = 0.28 * sin(t * 2.0 + rk * 6.28 + fk);
        vec2  sq = vec2(1.0 + wob, 1.0 / (1.0 + wob));
        float sway = 0.006 * sin(t * 1.8 + fk + seed * 6.28);
        vec2  bc = P + vec2(sway, 0.012 + lp * 0.085);      // rise above the head
        vec2  d  = (uv - bc) * vec2(aspect, 1.0) * sq;
        float dn = length(d) / sz;
        float er = (dn - 0.80) / 0.16;
        float rim = exp(-er * er);                          // hollow ring
        vec2  hp = (d / sz) - vec2(-0.30, 0.30);
        float hi = exp(-(length(hp) / 0.26) * (length(hp) / 0.26));
        float a  = smoothstep(0.0, 0.10, lp);               // fade IN only (no fade-out)
        acc += (rim * 0.85 + hi * 0.9) * a * step(dn, 1.25);
    }
    return acc;
}

void main()
{
    float s = clamp(u_strength, 0.0, 1.0);
    float aspect = u_res.x / max(u_res.y, 1.0);

    // 0. refraction: gently wobble the sample UV (looking through moving water)
    vec2 uv = v_uv;
    if (u_refract > 0.0)
    {
        float w = 0.0018 * u_refract;
        uv += vec2(sin(v_uv.y * 11.0 + u_time * 1.3),
                   cos(v_uv.x * 13.0 + u_time * 1.1)) * w;
    }
    vec3 c = texture(u_scene, uv).rgb;
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

    // 4. caustics - bright light-net on lit surfaces
    if (u_caustics > 0.0)
    {
        float net = caustics(v_uv, u_time);
        c += net * surf * (0.45 + 0.55 * v_uv.y)
             * u_caustics * vec3(0.22, 0.46, 0.52);
    }

    // 5. marine snow - slow drifting motes (two parallax layers)
    if (u_snow > 0.0)
    {
        float sn  = particleLayer(v_uv, u_time, 26.0, vec2(0.010, 0.018), 0.045, 1.0, aspect);
        sn += 0.6 * particleLayer(v_uv, u_time, 40.0, vec2(-0.014, 0.026), 0.035, 7.0, aspect);
        c += sn * u_snow * vec3(0.55, 0.68, 0.72);
    }

    // 6. bubbles - hollow, rising from vents distributed across the SEABED (the
    //    `surf` mask keeps them on the floor, not the bottom edge of the screen)
    // seabed vents (OFF by default - screen-anchored; re-enable once world-anchored)
    if (u_bubbles > 0.0)
    {
        float bz  = bubbleLayer(v_uv, u_time,  7.0, 0.10, 0.30, 21.0, aspect, 0.10);
        bz += 0.8 * bubbleLayer(v_uv, u_time, 11.0, 0.13, 0.22, 33.0, aspect, 0.09);
        c += bz * surf * u_bubbles * vec3(0.60, 0.84, 0.96);
    }
    // small breathing puffs from each visible aquanaut (our HD replacement for the
    // vanilla sprite bubbles, which are suppressed on this build)
    if (u_unitbub > 0.0)
    {
        for (int i = 0; i < 12; ++i)
        {
            if (i >= u_unitCount) break;
            c += unitBubbles(v_uv, u_unitPos[i], u_unitBreath[i], float(i) * 0.137,
                             aspect, u_time)
                 * u_unitbub * vec3(0.62, 0.86, 0.98);
        }
    }

    // 7. vignette
    vec2  d   = v_uv - vec2(0.5, 0.54);
    float r   = length(vec2(d.x * 1.05, d.y * 1.25));
    float vig = (0.28 + 0.62 * s) * pow(clamp(r - 0.35, 0.0, 1.0), 2.0);
    c *= (1.0 - vig);

    out_color = vec4(c, 1.0);
}
