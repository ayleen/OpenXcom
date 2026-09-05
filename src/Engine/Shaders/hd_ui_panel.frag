/* hd_ui_panel.frag — HD UI styled-panel SDF painter.
 *
 * The Radar shape is intentionally procedural: one quad paints the circular
 * instrument, rings, 72 bearing ticks, scanline/grain texture, and live
 * clockwise sweep. Other shapes retain the original panel behavior.
 */
uniform vec2  u_quadSize;
uniform vec2  u_shapeOffset;
uniform vec2  u_size;
uniform float u_radius;
uniform int   u_shapeKind;
uniform float u_cutCorner;
uniform float u_borderWidth;
uniform vec4  u_borderColor;
uniform vec4  u_fillTop;
uniform vec4  u_fillBottom;
uniform vec2  u_gradDir;
uniform vec4  u_glowColor;
uniform float u_glowRadius;
uniform float u_opacity;
uniform vec4  u_radarRingColor;
uniform vec4  u_radarStrongRingColor;
uniform vec4  u_radarAxisColor;
uniform vec4  u_radarSweepColor;
uniform float u_radarSweepAngle;
uniform float u_radarTrailRadians;
uniform float u_radarRingWidth;
uniform float u_radarTickWidth;
uniform float u_radarGrainAmount;
uniform float u_radarSeed;
in vec2 v_uv;
out vec4 out_color;

const float kTau = 6.28318530718;

float sdRoundBox(vec2 p, vec2 b, float r)
{
	vec2 q = abs(p) - b + r;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float sdOpposingCutBox(vec2 p, vec2 b, float cutSize)
{
	vec2 local = p + b;
	float box = sdRoundBox(p, b, 0.0);
	float diagonal = 0.70710678;
	float topLeft = (cutSize - local.x - local.y) * diagonal;
	float bottomRight = (local.x + local.y - (2.0 * b.x + 2.0 * b.y - cutSize)) * diagonal;
	return max(box, max(topLeft, bottomRight));
}

float sdWarningTriangle(vec2 local, vec2 size)
{
	float slope = size.x / max(2.0 * size.y, 1.0);
	float normalizer = inversesqrt(1.0 + slope * slope);
	float centeredX = local.x - size.x * 0.5;
	float leftEdge = (-centeredX - slope * local.y) * normalizer;
	float rightEdge = (centeredX - slope * local.y) * normalizer;
	return max(max(leftEdge, rightEdge), max(-local.y, local.y - size.y));
}

float radarHash(vec2 p)
{
	return fract(sin(dot(p + u_radarSeed, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
	vec2 half_ = u_size * 0.5;
	vec2 p = v_uv * u_quadSize - u_shapeOffset - half_;
	vec2 local = p + half_;
	bool radar = u_shapeKind == 3;
	float radarRadius = 0.5 * min(u_size.x, u_size.y) * 0.95;
	float d = radar ? length(p) - radarRadius : sdRoundBox(p, half_, u_radius);
	if (u_shapeKind == 1)
		d = sdOpposingCutBox(p, half_, u_cutCorner);
	else if (u_shapeKind == 2)
		d = sdWarningTriangle(local, u_size);
	float aa = max(fwidth(d), 1e-4);

	float shapeMask  = 1.0 - smoothstep(-aa, aa, d);
	float coreMask   = 1.0 - smoothstep(-u_borderWidth - aa, -u_borderWidth + aa, d);
	float borderMask = clamp(shapeMask - coreMask, 0.0, 1.0);
	float t = clamp(dot(p, u_gradDir) / max(dot(u_size, abs(u_gradDir)), 1.0) + 0.5, 0.0, 1.0);
	vec4 fill = mix(u_fillTop, u_fillBottom, t);

	if (radar)
	{
		float radial = length(p);
		float normalizedRadius = radial / max(radarRadius, 1.0);
		// Center lift and darker edge are independent of rectangular panel
		// gradient, keeping the instrument circular in wide and portrait forms.
		fill = mix(u_fillTop, u_fillBottom,
			smoothstep(0.0, 1.0, normalizedRadius));

		float ringW = max(u_radarRingWidth, aa);
		float ringMask = 0.0;
		ringMask = max(ringMask, 1.0 - smoothstep(ringW, ringW + aa,
			abs(radial - radarRadius * (0.285 / 0.95))));
		ringMask = max(ringMask, 1.0 - smoothstep(ringW, ringW + aa,
			abs(radial - radarRadius * (0.51 / 0.95))));
		ringMask = max(ringMask, 1.0 - smoothstep(ringW, ringW + aa,
			abs(radial - radarRadius * (0.73 / 0.95))));
		float outerRing = 1.0 - smoothstep(ringW, ringW + aa,
			abs(radial - radarRadius));

		float angle = mod(atan(p.x, -p.y) + kTau, kTau);
		float tickStep = kTau / 72.0;
		float tickDelta = abs(mod(angle + tickStep * 0.5, tickStep) - tickStep * 0.5);
		float angularTick = 1.0 - smoothstep(
			u_radarTickWidth / max(radial, 1.0),
			u_radarTickWidth * 1.8 / max(radial, 1.0), tickDelta);
		float tickIndex = floor(angle / tickStep + 0.5);
		float major = 1.0 - step(0.5, mod(tickIndex, 6.0));
		float minorBand = smoothstep(radarRadius * 0.91, radarRadius * 0.92, radial)
			* (1.0 - smoothstep(radarRadius * 0.975, radarRadius * 0.98, radial));
		float majorBand = smoothstep(radarRadius * 0.87, radarRadius * 0.88, radial)
			* (1.0 - smoothstep(radarRadius * 0.975, radarRadius * 0.98, radial));
		float tickMask = angularTick * mix(minorBand, majorBand, major);

		float axisWidth = max(u_radarTickWidth, 1.0);
		float axisMask = max(
			1.0 - smoothstep(axisWidth, axisWidth + aa, abs(p.x)),
			1.0 - smoothstep(axisWidth, axisWidth + aa, abs(p.y)));

		float trail = clamp(u_radarTrailRadians, 0.01, kTau);
		float sweepDelta = mod(u_radarSweepAngle - angle + kTau, kTau);
		float sweepMask = 1.0 - smoothstep(0.0, trail, sweepDelta);
		float beamMask = 1.0 - smoothstep(0.0, 0.018, sweepDelta);
		float beamGrid = clamp(sweepMask * 0.45 + beamMask * 0.55, 0.0, 1.0);

		vec3 gridColor = mix(u_radarRingColor.rgb, u_radarStrongRingColor.rgb, outerRing);
		float gridAlpha = max(ringMask * u_radarRingColor.a,
			outerRing * u_radarStrongRingColor.a);
		gridColor = mix(gridColor, u_radarStrongRingColor.rgb, tickMask);
		gridAlpha = max(gridAlpha, tickMask * u_radarStrongRingColor.a);
		gridColor = mix(gridColor, u_radarAxisColor.rgb, axisMask);
		gridAlpha = max(gridAlpha, axisMask * u_radarAxisColor.a);
		// Sweep lifts the otherwise restrained grid, rather than replacing it.
		gridColor = mix(gridColor, u_radarSweepColor.rgb, beamGrid * 0.65);
		gridAlpha = max(gridAlpha, beamGrid * u_radarSweepColor.a);
		fill.rgb = mix(fill.rgb, gridColor, clamp(gridAlpha, 0.0, 1.0));
		fill.rgb = mix(fill.rgb, u_radarSweepColor.rgb,
			clamp(sweepMask * u_radarSweepColor.a * 0.35, 0.0, 1.0));
		fill.rgb = mix(fill.rgb, u_radarSweepColor.rgb, beamMask * 0.9);

		float grain = (radarHash(floor(gl_FragCoord.xy)) - 0.5) * u_radarGrainAmount;
		float scanline = (sin(gl_FragCoord.y * 3.14159265) - 0.5) * 0.012;
		fill.rgb += (grain + scanline) * vec3(0.35, 0.9, 0.65);
	}

	vec4 shapeCol = mix(fill, u_borderColor, borderMask);
	float shapeA = shapeCol.a * shapeMask;
	float glowA = 0.0;
	if (u_glowRadius > 0.0)
	{
		float g = clamp(1.0 - d / u_glowRadius, 0.0, 1.0);
		glowA = u_glowColor.a * g * (1.0 - shapeMask);
	}
	vec3 rgb = mix(u_glowColor.rgb, shapeCol.rgb, shapeMask);
	out_color = vec4(rgb, max(shapeA, glowA) * u_opacity);
}
