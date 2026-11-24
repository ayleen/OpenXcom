#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace OpenXcom
{
constexpr size_t StrategicGraphMonths = 12;

template<class T>
T strategicGraphValue(const std::vector<T> &history, size_t monthsAgo)
{
	if (monthsAgo >= StrategicGraphMonths || monthsAgo >= history.size()) return {};
	return history[history.size() - 1 - monthsAgo];
}

struct StrategicGraphAxis
{
	double low = 0, high = 1;
	double fraction(double value) const
	{
		return std::clamp((value - low) / (high - low), 0.0, 1.0);
	}
};

inline StrategicGraphAxis strategicGraphAxis(double minimum, double maximum, int zoom, double step)
{
	// A zero baseline is retained for positive and negative activity.
	minimum = std::min(0.0, minimum);
	maximum = std::max(0.0, maximum);
	while (std::ceil(maximum / step) - std::floor(minimum / step) > 9.0)
		step *= 2;
	const double low = std::floor(minimum / step) * step;
	const double scale = std::clamp(zoom, 5, 100) / 100.0;
	return {low * scale, (low + 9 * step) * scale};
}

struct StrategicGraphSeries
{
	std::array<double, StrategicGraphMonths> values{};
	int color = 0;
	bool visible = false;
};
}
