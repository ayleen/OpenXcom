#ifdef __EMSCRIPTEN__

#include "RuleVoiceProfile.h"

#include "../Engine/Exception.h"

#include <algorithm>
#include <array>

namespace OpenXcom
{
namespace
{

constexpr std::array<const char *, 19> KNOWN_EVENTS = {{
	"selected", "reselected", "annoyed_1", "annoyed_2", "annoyed_3",
	"move_ack", "weapon_ready", "out_of_ammo", "alien_spotted",
	"grenade_throw", "hostile_hit", "miss", "hostile_kill", "friendly_hit",
	"civilian_hit", "wounded", "panic", "death", "flee",
}};

bool validPathTemplate(const std::string &value)
{
	return value.find("{profile}") != std::string::npos
		&& value.find("{line}") != std::string::npos;
}

}

RuleVoiceProfile::RuleVoiceProfile(const std::string &id) : _id(id)
{
}

void RuleVoiceProfile::load(const YAML::YamlNodeReader &reader)
{
	reader.tryRead("locale", _locale);
	reader.tryRead("baseLocale", _baseLocale);
	reader.tryRead("unitClass", _unitClass);
	reader.tryRead("gender", _gender);
	reader.tryRead("pack", _pack);
	reader.tryRead("fallbackProfile", _fallbackProfile);
	reader.tryRead("dryPathTemplate", _dryPathTemplate);
	reader.tryRead("wetPathTemplate", _wetPathTemplate);

	if (_id.empty() || _locale.empty() || _unitClass.empty() || _pack.empty())
	{
		throw Exception("voiceProfiles: id, locale, unitClass and pack are required");
	}
	if (_unitClass != "diver" && _unitClass != "civilian")
	{
		throw Exception("voiceProfiles[" + _id + "]: unknown unitClass '" + _unitClass + "'");
	}
	if (_gender != "male" && _gender != "female")
	{
		throw Exception("voiceProfiles[" + _id + "]: gender must be male or female");
	}
	if (!validPathTemplate(_dryPathTemplate))
	{
		throw Exception("voiceProfiles[" + _id + "]: dryPathTemplate must contain {profile} and {line}");
	}
	if (!_wetPathTemplate.empty() && !validPathTemplate(_wetPathTemplate))
	{
		throw Exception("voiceProfiles[" + _id + "]: wetPathTemplate must contain {profile} and {line}");
	}

	if (const auto &events = reader["events"])
	{
		for (const auto &eventReader : events.children())
		{
			const std::string event = eventReader.readKey<std::string>();
			if (!isKnownEvent(event))
			{
				throw Exception("voiceProfiles[" + _id + "]: unknown event '" + event + "'");
			}

			VoiceEventRule rule;
			eventReader.tryRead("lines", rule.lines);
			eventReader.tryRead("weights", rule.weights);
			eventReader.tryRead("priority", rule.priority);
			eventReader.tryRead("probability", rule.probability);
			eventReader.tryRead("cooldownMs", rule.cooldownMs);
			if (rule.lines.empty())
			{
				throw Exception("voiceProfiles[" + _id + "].events[" + event + "]: lines must not be empty");
			}
			if (rule.weights.empty())
			{
				rule.weights.assign(rule.lines.size(), 1);
			}
			if (rule.weights.size() != rule.lines.size()
				|| std::any_of(rule.weights.begin(), rule.weights.end(), [](int weight) { return weight <= 0; }))
			{
				throw Exception("voiceProfiles[" + _id + "].events[" + event + "]: weights must be positive and match lines");
			}
			if (rule.probability < 0 || rule.probability > 100 || rule.cooldownMs < 0)
			{
				throw Exception("voiceProfiles[" + _id + "].events[" + event + "]: invalid probability or cooldownMs");
			}
			_events[event] = std::move(rule);
		}
	}
	if (_events.empty())
	{
		throw Exception("voiceProfiles[" + _id + "]: events must not be empty");
	}
}

const VoiceEventRule *RuleVoiceProfile::getEvent(const std::string &event) const
{
	const auto found = _events.find(event);
	return found == _events.end() ? nullptr : &found->second;
}

bool RuleVoiceProfile::isCompatible(const std::string &locale,
	const std::string &unitClass, const std::string &gender) const
{
	return _locale == locale && _unitClass == unitClass && _gender == gender;
}

bool RuleVoiceProfile::isKnownEvent(const std::string &event)
{
	return std::find(KNOWN_EVENTS.begin(), KNOWN_EVENTS.end(), event) != KNOWN_EVENTS.end();
}

}

#endif
