#pragma once

#ifdef __EMSCRIPTEN__

#include "../Engine/Yaml.h"

#include <map>
#include <string>
#include <vector>

namespace OpenXcom
{

struct VoiceEventRule
{
	std::vector<std::string> lines;
	std::vector<int> weights;
	int priority = 0;
	int probability = 100;
	int cooldownMs = 4000;
};

/**
 * Data-only voice identity loaded from the Calypso voice ruleset.
 *
 * Audio stays outside the ruleset and is resolved lazily through the web voice
 * pack. Stable profile IDs are the only values persisted in savegames.
 */
class RuleVoiceProfile
{
private:
	std::string _id;
	std::string _locale;
	std::string _baseLocale;
	std::string _unitClass;
	std::string _gender;
	std::string _pack;
	std::string _fallbackProfile;
	std::string _dryPathTemplate;
	std::string _wetPathTemplate;
	std::map<std::string, VoiceEventRule> _events;

public:
	explicit RuleVoiceProfile(const std::string &id = std::string());
	void load(const YAML::YamlNodeReader &reader);

	const std::string &getId() const { return _id; }
	const std::string &getLocale() const { return _locale; }
	const std::string &getBaseLocale() const { return _baseLocale; }
	const std::string &getUnitClass() const { return _unitClass; }
	const std::string &getGender() const { return _gender; }
	const std::string &getPack() const { return _pack; }
	const std::string &getFallbackProfile() const { return _fallbackProfile; }
	const std::string &getDryPathTemplate() const { return _dryPathTemplate; }
	const std::string &getWetPathTemplate() const { return _wetPathTemplate; }
	const std::map<std::string, VoiceEventRule> &getEvents() const { return _events; }
	const VoiceEventRule *getEvent(const std::string &event) const;

	bool isCompatible(const std::string &locale, const std::string &unitClass,
		const std::string &gender) const;
	static bool isKnownEvent(const std::string &event);
};

}

#endif
