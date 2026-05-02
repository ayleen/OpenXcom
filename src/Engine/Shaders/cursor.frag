// cursor.frag — Phase 15: SDF rounded-diamond cursor box + soft glow.
//
// Renders a single instanced quad as a resolution-independent selector
// box using a signed-distance-field approximation of a rounded diamond.
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().

precision highp float;

in  vec2       v_localUV;
flat in vec4   v_color;
flat in vec4   v_params;   // .x thickness  .y radius  .z glowWidth  .w phase [0..1]

out vec4 fragColor;

// SDF: rounded diamond in local UV space [-1..1].
// Returns negative inside the shape, 0 on the boundary, positive outside.
float sdRoundedDiamond(vec2 p, float r)
{
    // Shrink by radius, compute L1 distance, then expand back.
    vec2 q = abs(p);
    // Normalise so that the outer diamond tip is at UV magnitude 1.
    float d = q.x + q.y - 1.0;
    // Subtract radius so corners are rounded (the SDF is clamped-rounded).
    return d - r * (1.0 - smoothstep(0.0, 0.5, max(q.x, q.y)));
}

void main()
{
    float thickness = v_params.x;
    float radius    = v_params.y;
    float glowW     = v_params.z;
    float phase     = v_params.w;   // 0..1 pulse

    float d = sdRoundedDiamond(v_localUV, radius);

    // Outline: a ring of width `thickness` centered on the SDF zero-crossing.
    // Pulse modulates the ring width so the yellow box visibly throbs.
    float t = thickness * (0.7 + 0.3 * phase);
    float outline = smoothstep(t, 0.0, abs(d));

    // Soft outer glow decays from the outer edge of the diamond.
    float glow = smoothstep(glowW, 0.0, max(d, 0.0)) * 0.4;

    float alpha = clamp(outline + glow, 0.0, 1.0);
    if (alpha < 0.01) discard;
    fragColor = vec4(v_color.rgb, v_color.a * alpha);
}
