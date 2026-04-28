// globe_sphere.vert — Phase 8c: HD globe fullscreen-quad vertex shader.
// Passes viewport pixel coordinates to the fragment shader.
in vec2 a_pos;   // NDC [-1,+1]
in vec2 a_uv;    // unused; present so VAO layout matches other passes

uniform vec2 u_viewportSize;

out vec2 v_pixel;

void main()
{
    gl_Position = vec4(a_pos, 0.0, 1.0);
    // Convert NDC → pixel coords (0,0 = top-left, consistent with SDL)
    v_pixel = (a_pos * vec2(0.5, -0.5) + vec2(0.5, 0.5)) * u_viewportSize;
}
