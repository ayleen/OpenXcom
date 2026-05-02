// cursor.vert — Phase 15: instanced unit-quad vertex shader for SDF cursor.
//
// Draws one screen-aligned quad per cursor-box instance.  Instance data
// supplies the screen-space rect, color, and SDF params; the static VBO
// supplies the unit-quad corners [-1..1].
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().

layout(location = 0) in vec2 a_unitPos;   // unit quad corner [-1..1]
layout(location = 1) in vec4 a_xywh;      // display-pixel rect (x, y, w, h), divisor 1
layout(location = 2) in vec4 a_color;     // RGBA outline color, divisor 1
layout(location = 3) in vec4 a_params;    // (thickness, radius, glowWidth, phase), divisor 1

uniform vec2 u_screenSize;               // display resolution (px)

out vec2       v_localUV;
flat out vec4  v_color;
flat out vec4  v_params;

void main()
{
    // Map unit corner to pixel position inside the instance rect.
    vec2 px = a_xywh.xy + (a_unitPos * 0.5 + 0.5) * a_xywh.zw;
    vec2 ndc = (px / u_screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    v_localUV = a_unitPos;   // passes [-1..1] to fragment for SDF evaluation
    v_color   = a_color;
    v_params  = a_params;
}
