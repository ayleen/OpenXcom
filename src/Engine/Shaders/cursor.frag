// cursor.frag — Phase 16: 4-tip animated SDF cursor markers + AP arc ring.
//
// Two modes dispatched by the sign of v_params.x:
//
// MARKER mode (v_params.x >= 0):
//   Renders four small diamond markers near the N/S/E/W isometric tile tips.
//   All four markers are equidistant from the tile centre in pixel space
//   (v_aspect correction) and breathe via a sin-based animation.
//   a_params: (markerSize, baseFrac, animFrac, phase)
//
// RING mode (v_params.x < 0):
//   Renders a partial arc ring (AP gauge) around the selected unit.
//   The arc fills clockwise from 12 o'clock proportional to remaining TU.
//   A faint full-circle background shows the max TU boundary.
//   a_params: (-innerRadius, ringWidth, arcFraction[0..1], unused)
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().

precision highp float;

in  vec2        v_localUV;
flat in float   v_aspect;    // quad w/h — corrects X so 1 iso unit = h/2 px
flat in vec4    v_color;
flat in vec4    v_params;

out vec4 fragColor;

// L1 SDF: solid diamond centred at `c` with half-size `sz` in iso space.
float sdDiamond(vec2 p, vec2 c, float sz)
{
    vec2 d = abs(p - c);
    return (d.x + d.y) - sz;
}

void main()
{
    float inv = 1.0 / v_aspect;
    vec2 p = vec2(v_localUV.x * inv, v_localUV.y);

    // ── Ring (AP gauge) mode ────────────────────────────────────────────────
    if (v_params.x < -0.01)
    {
        float innerR  = -v_params.x;          // inner radius in iso units
        float ringW   =  v_params.y;           // ring width in iso units
        float arcFrac =  v_params.z;           // [0..1] fraction of TU remaining

        float r    = length(p);
        float ring = abs(r - (innerR + ringW * 0.5)) - ringW * 0.5;

        // Angle: 0 = top (12 o'clock), increasing clockwise.
        // atan(x, -y): at top p=(0,-1) → atan(0,1)=0; at right p=(inv,0) → π/2.
        float angle     = atan(p.x, -p.y);
        float normAngle = mod(angle / 6.28318530718 + 1.0, 1.0);

        // Faint background ring (full circle — shows TU maximum).
        float bgAlpha  = smoothstep(0.025, -0.025, ring) * 0.18;

        // Bright arc proportional to remaining TU.
        float arcMask  = step(normAngle, arcFrac);
        float arcAlpha = smoothstep(0.025, -0.025, ring) * arcMask;
        float glow     = smoothstep(0.09, 0.0, max(ring, 0.0)) * arcMask * 0.35;

        float alpha = clamp(arcAlpha + bgAlpha + glow, 0.0, 1.0);
        if (alpha < 0.01) discard;
        fragColor = vec4(v_color.rgb, v_color.a * alpha);
        return;
    }

    // ── 4-tip marker mode ───────────────────────────────────────────────────
    float markerSize = v_params.x;
    float baseFrac   = v_params.y;
    float animFrac   = v_params.z;
    float phase      = v_params.w;

    // Animated fraction: all 4 markers move together (breathing effect).
    float frac = baseFrac + animFrac * sin(phase * 6.28318530718);

    // Marker centres in iso space — equidistant from tile centre in pixels.
    vec2 cN = vec2(0.0,       -frac);
    vec2 cS = vec2(0.0,       +frac);
    vec2 cW = vec2(-frac * inv, 0.0);
    vec2 cE = vec2(+frac * inv, 0.0);

    float d = min(min(sdDiamond(p, cN, markerSize),
                      sdDiamond(p, cS, markerSize)),
                  min(sdDiamond(p, cW, markerSize),
                      sdDiamond(p, cE, markerSize)));

    float alpha = smoothstep(0.025, -0.025, d);
    float glow  = smoothstep(0.10, 0.0, max(d, 0.0)) * 0.40;

    alpha = clamp(alpha + glow, 0.0, 1.0);
    if (alpha < 0.01) discard;
    fragColor = vec4(v_color.rgb, v_color.a * alpha);
}
