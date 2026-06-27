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
    float edgeAlpha = 1.0 - smoothstep(0.985, 1.0, r);

    // Orthographic inverse: pixel → unit-sphere surface normal in view space.
    // View space matches Globe::polarToCart(): +X right, +Y down, +Z front.
    float nz      = sqrt(max(0.0, 1.0 - r * r));
    vec3  n_view  = vec3(d.x / u_globeRadius, d.y / u_globeRadius, nz);

    // Rotate to world space using camera orientation.
    vec3 n_world = applyCameraRotation(n_view, -u_camLat, u_camLon);

    // World normal → equirectangular UV.
    float lat  = asin(clamp(n_world.y, -1.0, 1.0));
    // Bias atan only at the exact pole singularity; preserves accuracy elsewhere.
    float bias = (abs(n_world.x) + abs(n_world.z) < 1e-7) ? 1e-5 : 0.0;
    float lon  = atan(n_world.x, n_world.z + bias);           // [-π, +π]
    // OXCE xcom2 globe rules use inverted latitude compared to real-world
    // equirectangular imagery (London is stored near -49°, not +51°).
    // Keep geometry/clicks in game coordinates, but sample NASA textures with
    // latitude flipped so visual land/water matches the gameplay map.
    vec2  uv  = vec2((lon + 3.14159265) / 6.28318530,        // [0, 1]
                     (1.57079633 + lat) / 3.14159265);        // [0, 1]

    // Sample all layers at the selected mip level.
    vec3  bathy  = textureLod(u_bathymetry, uv, u_mipLevel).rgb;
    vec3  diff   = textureLod(u_diffuse,    uv, u_mipLevel).rgb;
    vec3  night  = textureLod(u_night,      uv, u_mipLevel).rgb;
    // Cloud layer drifts longitudinally; GL_REPEAT handles seamless wrap.
    // One wrap is ~37 minutes: visible in play, calm enough for Geoscape.
    vec2  cloudUV = vec2(uv.x + u_time * 0.00045, uv.y);
    vec4  cloud   = textureLod(u_clouds, cloudUV, u_mipLevel);

    // Stylized TFTD composite.  Real satellite colour is too bright and
    // documentary-looking next to the low-res Geoscape UI, so keep the ocean
    // dominant and treat land as a muted overlay until a real land mask ships.
    float landWarmth = diff.r * 1.15 + diff.g * 0.85 - diff.b * 1.25;
    float landMask = smoothstep(0.10, 0.28, landWarmth);

    vec3 ocean = bathy * vec3(0.16, 0.42, 0.62) + vec3(0.00, 0.018, 0.040);
    vec3 dryLand = diff * vec3(0.48, 0.56, 0.50) + vec3(0.00, 0.015, 0.020);

    // Preserve a hint of real tropical greenery.  The mask is intentionally
    // soft: enough to make Amazon/Congo/Indonesia read green, not enough to
    // pull the globe back into raw satellite-photo colours.
    float diffMax = max(max(diff.r, diff.g), diff.b);
    float diffMin = min(min(diff.r, diff.g), diff.b);
    float diffLuma = dot(diff, vec3(0.299, 0.587, 0.114));
    float diffSat = diffMax - diffMin;
    float greenHue =
        smoothstep(-0.015, 0.070, diff.g - diff.r * 0.88) *
        smoothstep(-0.030, 0.075, diff.g - diff.b * 0.78);
    float notSnow = (1.0 - smoothstep(0.50, 0.74, diffLuma)) * smoothstep(0.045, 0.155, diffSat);
    float vegetation = landMask * greenHue * notSnow;
    vec3 greenLand = diff * vec3(0.30, 0.92, 0.50) + vec3(0.00, 0.055, 0.025);
    vec3 land = mix(dryLand, greenLand, vegetation * 0.78);
    vec3 surface = mix(ocean, land, landMask);

    // Desaturate and cool the whole globe toward TFTD's sonar/navy mood.
    float luma = dot(surface, vec3(0.299, 0.587, 0.114));
    surface = mix(vec3(luma), surface, 0.72);
    surface *= vec3(0.72, 0.92, 1.05);

    // Day / night terminator.  Keep it wider than the physical value: a
    // razor-sharp terminator looks like a black stripe at Geoscape scale.
    float sunDot   = dot(n_world, u_sunDir);
    float dayFactor = smoothstep(-0.46, 0.24, sunDot);

    // Cloud opacity comes from the WebP alpha channel.  The source cloud map is
    // very dense, so compress it hard instead of washing the globe to white.
    float cloudDensity = smoothstep(0.34, 0.96, cloud.a) * 0.24;
    vec3 cloudColor = vec3(0.66, 0.86, 0.90);

    vec3 daySide = mix(surface, cloudColor, cloudDensity * dayFactor) * (0.42 + dayFactor * 0.76);
    vec3 nightSurface = surface * vec3(0.42, 0.48, 0.56);
    vec3 nightSide = (nightSurface + vec3(0.005, 0.030, 0.055) + night * vec3(0.18, 0.62, 0.78)) * (1.0 - dayFactor);

    // Darken the limb and add a thin cyan atmospheric rim inside the disk.
    float limb = smoothstep(0.03, 0.55, nz);
    vec3 rim = vec3(0.00, 0.16, 0.20) * pow(1.0 - nz, 3.0);

    fragColor = vec4((daySide + nightSide) * (0.42 + limb * 0.58) + rim, edgeAlpha);
}
