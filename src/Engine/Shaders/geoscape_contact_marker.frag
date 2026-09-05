/* Physical contact affordance, anchored to the native marker snapshot.
 * Canonical Geoscape reference: 34/22 px rings and an 8 px diamond with a
 * 2 px border, inside the existing 44 px hit region. The original ruleset
 * sprite is drawn over this decoration; no visibility or hit policy changes.
 * Distances are logical pixels; derivatives keep edges sharp at every DPR.
 */
uniform float u_extent;
uniform vec4 u_color;
in vec2 v_uv;
out vec4 out_color;

void main()
{
    vec2 p = (v_uv - vec2(0.5)) * u_extent;
    float radial = length(p);
    float ringDistance = min(abs(radial - 16.5), abs(radial - 10.5)) - 0.5;
    float ringAA = max(fwidth(ringDistance), 1e-4);
    float rings = 1.0 - smoothstep(-ringAA, ringAA, ringDistance);

    float diamondDistance = (abs(p.x) + abs(p.y) - 5.65685425) * 0.70710678;
    float borderDistance = abs(diamondDistance + 1.0) - 1.0;
    float diamondAA = max(fwidth(diamondDistance), 1e-4);
    float diamond = 1.0 - smoothstep(-diamondAA, diamondAA, borderDistance);
    // A dark keyline preserves contrast over bright coastlines and land.
    float keyline = 0.85 * (1.0 - smoothstep(-diamondAA, diamondAA, borderDistance - 1.0));
    float ink = max(diamond, rings * 0.24);
    float alpha = ink + keyline * (1.0 - ink);
    out_color = vec4(u_color.rgb * ink / max(alpha, 1e-4), alpha * u_color.a);
}
