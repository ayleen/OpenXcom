// tile_atlas_rgba.frag — Phase 12.3: RGBA atlas path, no shade-table lookup.
//
// Samples the atlas as RGBA8 (uploaded with GL_LINEAR filter so 256×320 source
// tiles downsample smoothly).  Applies a simple linear darkening factor in place
// of the palette shade-table used by the palette variant.
//
// shadeFactor calibration: linear 1 - shade/15 * 0.6 is the starting point;
// replace with a 16-element LUT uniform if visual mismatch next to
// palette-rendered tiles is unacceptable at night missions (shade > 8).
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().

uniform sampler2D u_atlas;
uniform float     u_animFrame;
uniform vec2      u_tileUVSize;

in vec2  v_uv;
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
    if (c.a < 0.5) discard;

    float k = 1.0 - (v_shade / 15.0) * 0.6;
    fragColor = vec4(c.rgb * k, c.a);
}
