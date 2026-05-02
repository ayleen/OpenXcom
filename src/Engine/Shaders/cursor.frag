// cursor.frag — Phase 16: 4-tip animated SDF cursor markers.
//
// Renders four small diamond markers near the N/S/E/W tips of the isometric
// tile.  All four markers are placed at the same pixel distance from the tile
// centre (isotropic space via v_aspect correction), so they appear identical
// in size and shape regardless of tile dimensions.
//
// Animation: markers oscillate toward/away from the tile centre ("breathing").
//
// a_params layout (from vertex shader):
//   .x  markerSize  — half-size of each diamond in iso units (typ. 0.10)
//   .y  baseFrac    — base fraction of iso tip distance (typ. 0.72)
//   .z  animFrac    — oscillation amplitude in fraction units (typ. 0.08)
//   .w  phase       — animation phase [0..1], driven by CPU wallclock
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
    float markerSize = v_params.x;
    float baseFrac   = v_params.y;
    float animFrac   = v_params.z;
    float phase      = v_params.w;

    // Iso space: scale X by (1/aspect) so that 1 unit = h/2 px on both axes.
    float inv = 1.0 / v_aspect;
    vec2 p = vec2(v_localUV.x * inv, v_localUV.y);

    // Animated fraction: all 4 markers move together (breathing effect).
    float frac = baseFrac + animFrac * sin(phase * 6.28318530718);

    // Marker centres in iso space — equidistant from tile centre in pixels.
    //   N/S along Y axis:    centre at (0, ±frac)
    //   W/E along X axis:    centre at (±frac·inv, 0)  [inv scales to same px dist]
    vec2 cN = vec2(0.0,       -frac);
    vec2 cS = vec2(0.0,       +frac);
    vec2 cW = vec2(-frac * inv, 0.0);
    vec2 cE = vec2(+frac * inv, 0.0);

    float d = min(min(sdDiamond(p, cN, markerSize),
                      sdDiamond(p, cS, markerSize)),
                  min(sdDiamond(p, cW, markerSize),
                      sdDiamond(p, cE, markerSize)));

    // Filled marker core with 1-px soft edge (AA).
    float alpha = smoothstep(0.025, -0.025, d);

    // Soft glow that bleeds out beyond the marker edge.
    float glow = smoothstep(0.10, 0.0, max(d, 0.0)) * 0.40;

    alpha = clamp(alpha + glow, 0.0, 1.0);
    if (alpha < 0.01) discard;
    fragColor = vec4(v_color.rgb, v_color.a * alpha);
}
