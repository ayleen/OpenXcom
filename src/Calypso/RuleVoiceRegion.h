#pragma once

#ifdef __EMSCRIPTEN__

#include "CalypsoVoiceRegion.h"
#include "../Engine/Yaml.h"

namespace OpenXcom
{

class RuleVoiceRegion
{
private:
	CalypsoVoiceRegionDescriptor _descriptor;

public:
	explicit RuleVoiceRegion(const std::string &id = std::string());
	void load(const YAML::YamlNodeReader &reader);
	const CalypsoVoiceRegionDescriptor &getDescriptor() const { return _descriptor; }
};

}

#endif
