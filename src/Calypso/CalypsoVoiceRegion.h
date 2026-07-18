#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace OpenXcom
{

struct CalypsoVoiceRegionArea
{
	double lonMin = 0.0;
	double lonMax = 0.0;
	double latMin = 0.0;
	double latMax = 0.0;

	bool contains(double lon, double lat) const
	{
		const bool inLon = lonMin <= lonMax
			? lon >= lonMin && lon < lonMax
			: (lon >= lonMin || lon < lonMax);
		const bool inLat = lat > 0.0
			? lat > latMin && lat <= latMax
			: lat >= latMin && lat < latMax;
		return inLon && inLat;
	}
};

struct CalypsoVoiceRegionDescriptor
{
	std::string id;
	std::string locale;
	int priority = 0;
	std::vector<CalypsoVoiceRegionArea> areas;
};

inline std::string calypsoResolveCivilianVoiceLocale(
	const std::vector<CalypsoVoiceRegionDescriptor> &regions,
	double lon, double lat)
{
	constexpr double TWO_PI = 6.28318530717958647692;
	lon = std::fmod(lon, TWO_PI);
	if (lon < 0.0)
	{
		lon += TWO_PI;
	}
	const CalypsoVoiceRegionDescriptor *selected = nullptr;
	for (const CalypsoVoiceRegionDescriptor &region : regions)
	{
		const bool contains = std::any_of(region.areas.begin(), region.areas.end(),
			[&](const CalypsoVoiceRegionArea &area) { return area.contains(lon, lat); });
		if (!contains)
		{
			continue;
		}
		if (!selected || region.priority > selected->priority
			|| (region.priority == selected->priority && region.id < selected->id))
		{
			selected = &region;
		}
	}
	return selected && !selected->locale.empty() ? selected->locale : "en";
}

}
