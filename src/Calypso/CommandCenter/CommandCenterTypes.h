#pragma once
/*
 * Command Center -- base value types (normative spec 2026-08-28, s.6).
 *
 * Pure, dependency-free data types for the Command Center UI. The spec's
 * "adapt to your engine" note applies: these mirror the normative shapes so
 * the layout math reads exactly like the specification, while the renderer
 * converts to the engine's own primitives at the boundary.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{
namespace CommandCenter
{

struct Vec2
{
	float x = 0.0f;
	float y = 0.0f;
};

struct Size2
{
	float width = 0.0f;
	float height = 0.0f;
};

struct RectF
{
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;

	float right() const { return x + width; }
	float bottom() const { return y + height; }
	bool contains(Vec2 p) const
	{
		return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
	}
};

struct InsetsF
{
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
};

struct Color8
{
	std::uint8_t r = 0;
	std::uint8_t g = 0;
	std::uint8_t b = 0;
	std::uint8_t a = 255;
};

inline RectF inset(const RectF& rect, float amount)
{
	return { rect.x + amount, rect.y + amount,
		rect.width - amount * 2.0f, rect.height - amount * 2.0f };
}

inline RectF inset(const RectF& rect, const InsetsF& in)
{
	return { rect.x + in.left, rect.y + in.top,
		rect.width - in.left - in.right, rect.height - in.top - in.bottom };
}

inline RectF inflate(const RectF& rect, float amount)
{
	return inset(rect, -amount);
}

inline float clampFloat(float value, float minValue, float maxValue)
{
	return std::max(minValue, std::min(value, maxValue));
}

inline float lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom
