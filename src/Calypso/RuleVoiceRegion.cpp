#ifdef __EMSCRIPTEN__

#include "RuleVoiceRegion.h"

#include "../Engine/Exception.h"
#include "../fmath.h"

namespace OpenXcom
{

RuleVoiceRegion::RuleVoiceRegion(const std::string &id)
{
	_descriptor.id = id;
}

void RuleVoiceRegion::load(const YAML::YamlNodeReader &reader)
{
	reader.tryRead("locale", _descriptor.locale);
	reader.tryRead("priority", _descriptor.priority);
	_descriptor.areas.clear();
	for (const auto &area : reader["areas"].children())
	{
		CalypsoVoiceRegionArea parsed;
		parsed.lonMin = Deg2Rad(area[0].readVal<double>());
		parsed.lonMax = Deg2Rad(area[1].readVal<double>());
		parsed.latMin = Deg2Rad(area[2].readVal<double>());
		parsed.latMax = Deg2Rad(area[3].readVal<double>());
		if (parsed.latMin > parsed.latMax)
		{
			std::swap(parsed.latMin, parsed.latMax);
		}
		_descriptor.areas.push_back(parsed);
	}
	if (_descriptor.id.empty() || _descriptor.locale.empty()
		|| _descriptor.areas.empty())
	{
		throw Exception("voiceRegions: id, locale and at least one area are required");
	}
}

}

#endif
