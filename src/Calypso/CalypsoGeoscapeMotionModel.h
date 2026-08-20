#pragma once

#include <cmath>
#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

constexpr float GEOSCAPE_STAR_INTENSITY_MIN = 0.48f;
constexpr float GEOSCAPE_STAR_INTENSITY_MAX = 1.0f;
constexpr double GEOSCAPE_MOTION_PERIOD_SECONDS = 3408.0;

struct GeoscapeStarSample
{
	float x = 0.0f;
	float y = 0.0f;
	float intensity = 0.0f;
	bool operator==(const GeoscapeStarSample& other) const
	{
		return x == other.x && y == other.y && intensity == other.intensity;
	}
};

struct GeoscapeMotionSample
{
	double starPhase = 0.0;
	double cloudOffset = 0.0;
};

enum class GeoscapeContactState { Normal, Warning, Threat };

inline std::uint32_t calypsoGeoscapeHash(std::uint32_t v)
{
	v ^= v >> 16u;
	v *= 0x7feb352du;
	v ^= v >> 15u;
	v *= 0x846ca68bu;
	v ^= v >> 16u;
	return v;
}

inline GeoscapeMotionSample calypsoGeoscapeMotion(double presentationSeconds, bool reducedMotion)
{
	if (reducedMotion) return {0.5, 0.0};
	double wrapped = std::fmod(presentationSeconds, GEOSCAPE_MOTION_PERIOD_SECONDS);
	if (wrapped < 0.0) wrapped += GEOSCAPE_MOTION_PERIOD_SECONDS;
	const double phase = wrapped / GEOSCAPE_MOTION_PERIOD_SECONDS;
	return {phase, phase};
}

inline GeoscapeStarSample calypsoGeoscapeStar(
	std::uint32_t seed, std::uint32_t index, double presentationSeconds)
{
	const std::uint32_t h = calypsoGeoscapeHash(seed ^ (index * 0x9e3779b9u));
	const float x = static_cast<float>(h & 0xffffu) / 65535.0f;
	const float y = static_cast<float>((h >> 16u) & 0xffffu) / 65535.0f;
	const double localPhase = calypsoGeoscapeMotion(presentationSeconds, false).starPhase
		+ static_cast<double>((h >> 8u) & 0xffu) / 255.0;
	const float wave = static_cast<float>(0.5 + 0.5 * std::sin(localPhase * 6.28318530717958647692));
	return {x, y, GEOSCAPE_STAR_INTENSITY_MIN
		+ (GEOSCAPE_STAR_INTENSITY_MAX - GEOSCAPE_STAR_INTENSITY_MIN) * wave};
}

inline std::uint32_t calypsoGeoscapeContactTone(GeoscapeContactState state, bool /*reducedMotion*/)
{
	switch (state)
	{
	case GeoscapeContactState::Warning: return 0xE4B96AFFu;
	case GeoscapeContactState::Threat: return 0xF05B68FFu;
	case GeoscapeContactState::Normal:
	default: return 0x66F5D2FFu;
	}
}

} // namespace Calypso
} // namespace OpenXcom
