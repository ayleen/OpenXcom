// globe_sphere.frag — Phase 8c: HD globe multi-layer composite.
//
// Active uniforms:
//   u_globeCenter   vec2   — disk centre in pixel coords (top-left origin)
//   u_globeRadius   float  — disk radius in pixels
//   u_camLat        float  — Geoscape camera latitude  (radians, north +)
//   u_camLon        float  — Geoscape camera longitude (radians, east +)
//   u_sunDir        vec3   — unit sun direction vector in world frame
//   u_time          float  — seconds since startup (for cloud drift)
//   u_mipLevel      float  — 0..3; 0=8k, 3=1k; choose per zoom
//   u_bathymetry    sampler2D  — ocean depth / land colour layer (RGB)
//   u_diffuse       sampler2D  — Blue Marble surface (RGB)
//   u_night         sampler2D  — Black Marble night lights (RGB)
//   u_clouds        sampler2D  — cloud fraction (RGBA WebP; density from alpha channel)

in  vec2 v_pixel;
out vec4 fragColor;

uniform vec2      u_globeCenter;
uniform float     u_globeRadius;
uniform float     u_camLat;
uniform float     u_camLon;
uniform vec3      u_sunDir;
uniform float     u_time;
uniform float     u_mipLevel;

uniform sampler2D u_bathymetry;
uniform sampler2D u_diffuse;
uniform sampler2D u_night;
uniform sampler2D u_clouds;

// Rotate a view-space normal by camera (lat then lon) to world space.
// camLat: rotation around X axis (pitch south/north)
// camLon: rotation around Y axis (yaw east/west)
vec3 applyCameraRotation(vec3 n, float camLat, float camLon)
{
    // Rotate around X by camLat
    float sl = sin(camLat);  float cl = cos(camLat);
    float y1 =  cl * n.y - sl * n.z;
    float z1 =  sl * n.y + cl * n.z;
    n = vec3(n.x, y1, z1);

    // Rotate around Y by camLon
    float so = sin(camLon);  float co = cos(camLon);
    float x2 =  co * n.x + so * n.z;
    float z2 = -so * n.x + co * n.z;
    n = vec3(x2, n.y, z2);

    return n;
}

void main()
{
    // Distance from globe disk centre (pixel space)
    vec2  d = v_pixel - u_globeCenter;
    float r = length(d) / u_globeRadius;
    if (r > 1.0) discard;

    // Orthographic inverse: pixel → unit-sphere surface normal in view space.
    // View space: +X right, +Y down (SDL), +Z into screen.
    float nz      = sqrt(max(0.0, 1.0 - r * r));
    // Flip Y so north is up in world space despite SDL's down-Y convention.
    vec3  n_view  = vec3(d.x / u_globeRadius, -d.y / u_globeRadius, nz);

    // Rotate to world space using camera orientation.
    vec3 n_world = applyCameraRotation(n_view, -u_camLat, u_camLon);

    // World normal → equirectangular UV.
    float lat  = asin(clamp(n_world.y, -1.0, 1.0));
    // Bias atan only at the exact pole singularity; preserves accuracy elsewhere.
    float bias = (abs(n_world.x) + abs(n_world.z) < 1e-7) ? 1e-5 : 0.0;
    float lon  = atan(n_world.x, n_world.z + bias);           // [-π, +π]
    vec2  uv  = vec2((lon + 3.14159265) / 6.28318530,        // [0, 1]
                     (1.57079633 - lat) / 3.14159265);        // [0, 1]

    // Sample all layers at the selected mip level.
    vec3  bathy  = textureLod(u_bathymetry, uv, u_mipLevel).rgb;
    vec3  diff   = textureLod(u_diffuse,    uv, u_mipLevel).rgb;
    vec3  night  = textureLod(u_night,      uv, u_mipLevel).rgb;
    // Cloud layer drifts longitudinally; GL_REPEAT handles seamless wrap.
    vec2  cloudUV = vec2(uv.x + u_time * 0.0001, uv.y);
    vec4  cloud   = textureLod(u_clouds, cloudUV, u_mipLevel);

    // Surface: bathymetry is the ocean base; Blue Marble dominates on land
    // where it is brighter.  Multiply diffuse slightly to keep ocean blue.
    vec3 surface = max(bathy, diff * 0.95);

    // Day / night terminator — smooth 5° (~0.087 rad) transition.
    float sunDot   = dot(n_world, u_sunDir);
    float dayFactor = smoothstep(-0.087, 0.087, sunDot);

    // Cloud opacity comes from the WebP alpha channel — MODIS cloud fraction
    // is stored in the alpha channel (0 = clear, 1 = fully overcast).
    float cloudDensity = cloud.a;
    // Clouds invisible on the night side; render as near-white.
    vec3 daySide = mix(surface, vec3(1.0), cloudDensity * 0.9 * dayFactor);
    vec3 nightSide = night * (1.0 - dayFactor);

    fragColor = vec4(daySide + nightSide, 1.0);
}
