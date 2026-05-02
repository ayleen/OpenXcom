// cursor.vert — Phase 16: instanced unit-quad vertex shader for 4-tip SDF cursor.
//
// Draws one screen-aligned quad per cursor instance.  Instance data supplies
// the screen-space rect, color, and animation params; the static VBO supplies
// unit-quad corners [-1..1].
//
// Do NOT include #version or precision qualifiers here.
// The platform preamble is prepended by Shader::compile().

layout(location = 0) in vec2 a_unitPos;   // unit quad corner [-1..1]
layout(location = 1) in vec4 a_xywh;      // display-pixel rect (x, y, w, h), divisor 1
layout(location = 2) in vec4 a_color;     // RGBA marker color, divisor 1
layout(location = 3) in vec4 a_params;    // (markerSize, baseFrac, animFrac, phase), divisor 1

uniform vec2 u_screenSize;               // display resolution (px)

out vec2        v_localUV;
flat out float  v_aspect;   // w/h pixel ratio — used in frag for iso-space correction
flat out vec4   v_color;
flat out vec4   v_params;

void main()
{
    vec2 px = a_xywh.xy + (a_unitPos * 0.5 + 0.5) * a_xywh.zw;
    vec2 ndc = (px / u_screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    v_localUV = a_unitPos;
    v_aspect  = a_xywh.z / a_xywh.w;   // e.g. 32/40 = 0.8 for native TFTD tile
    v_color   = a_color;
    v_params  = a_params;
}
