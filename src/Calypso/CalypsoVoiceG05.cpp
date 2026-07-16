#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_G0_5)

#include "CalypsoVoiceG05.h"

#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"
#include "../Engine/Options.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Soldier.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace OpenXcom
{
namespace
{

enum class Event : std::size_t
{
	Selected,
	Reselected,
	Annoyed1,
	AlienSpotted,
	FriendlyHit,
	Wounded,
	Death,
	Count
};

struct ClipSpec
{
	Event event;
	SoldierGender gender;
	const char *relativePath;
};

struct EventCounter
{
	unsigned int attempted = 0;
	unsigned int fired = 0;
	unsigned int suppressed = 0;
};

constexpr const char *ROOT = "/game/calypso-voice-g0.5/";
constexpr Uint32 RESELECT_WINDOW_MS = 8000;

constexpr std::array<const char *, static_cast<std::size_t>(Event::Count)> EVENT_NAMES = {{
	"selected", "reselected", "annoyed_1", "alien_spotted",
	"friendly_hit", "wounded", "death"
}};

constexpr std::array<ClipSpec, 16> CLIPS = {{
	{Event::Selected, GENDER_MALE, "diver_en_m_custom_v2/STR_CALYPSO_VOICE_SELECTED_01.wav"},
	{Event::Selected, GENDER_MALE, "diver_en_m_custom_v2/STR_CALYPSO_VOICE_SELECTED_02.wav"},
	{Event::Selected, GENDER_MALE, "diver_en_m_custom_v2/STR_CALYPSO_VOICE_SELECTED_03.wav"},
	{Event::Selected, GENDER_MALE, "diver_en_m_custom_v2/STR_CALYPSO_VOICE_SELECTED_04.wav"},
	{Event::Reselected, GENDER_MALE, "diver_en_m_custom_v2/STR_CALYPSO_VOICE_RESELECTED_01.wav"},
	{Event::AlienSpotted, GENDER_MALE, "diver_en_m_custom_v2/STR_CALYPSO_VOICE_ALIEN_SPOTTED_01.wav"},
	{Event::Wounded, GENDER_MALE, "diver_en_m_custom_v2/STR_CALYPSO_VOICE_WOUNDED_01.wav"},
	{Event::Death, GENDER_MALE, "diver_en_m_custom_v2/STR_CALYPSO_VOICE_DEATH_01.wav"},
	{Event::Selected, GENDER_FEMALE, "diver_en_f_custom_kate/STR_CALYPSO_VOICE_SELECTED_01.wav"},
	{Event::Selected, GENDER_FEMALE, "diver_en_f_custom_kate/STR_CALYPSO_VOICE_SELECTED_02.wav"},
	{Event::Selected, GENDER_FEMALE, "diver_en_f_custom_kate/STR_CALYPSO_VOICE_SELECTED_03.wav"},
	{Event::Selected, GENDER_FEMALE, "diver_en_f_custom_kate/STR_CALYPSO_VOICE_SELECTED_04.wav"},
	{Event::Annoyed1, GENDER_FEMALE, "diver_en_f_custom_kate/STR_CALYPSO_VOICE_ANNOYED_1_01.wav"},
	{Event::AlienSpotted, GENDER_FEMALE, "diver_en_f_custom_kate/STR_CALYPSO_VOICE_ALIEN_SPOTTED_01.wav"},
	{Event::FriendlyHit, GENDER_FEMALE, "diver_en_f_custom_kate/STR_CALYPSO_VOICE_FRIENDLY_HIT_01.wav"},
	{Event::Death, GENDER_FEMALE, "diver_en_f_custom_kate/STR_CALYPSO_VOICE_DEATH_02.wav"}
}};

struct VariantBag
{
	std::vector<const ClipSpec *> order;
	std::size_t cursor = 0;
	unsigned int cycle = 0;
	const ClipSpec *last = nullptr;
};

struct PilotState
{
	bool active = false;
	std::map<std::string, Mix_Chunk *> chunks;
	std::set<std::string> failedLoads;
	std::array<EventCounter, static_cast<std::size_t>(Event::Count)> counters{};
	std::map<std::pair<int, Event>, Uint32> lastFired;
	std::set<std::pair<int, int>> spottedHostiles;
	std::set<int> voicedDeaths;
	std::map<int, BattleUnit *> pendingWounded;
	std::map<std::pair<int, Event>, VariantBag> variantBags;
	int repeatUnitId = -1;
	unsigned int repeatClicks = 0;
	Uint32 lastRepeatClick = 0;
	int currentPriority = 0;
};

PilotState g_state;

std::uint32_t nextVariantRandom(std::uint32_t &state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

const char *eventName(Event event)
{
	return EVENT_NAMES.at(static_cast<std::size_t>(event));
}

const char *genderName(SoldierGender gender)
{
	return gender == GENDER_FEMALE ? "female" : "male";
}

bool isDiver(const BattleUnit *unit, bool requirePlayerControl = true)
{
	return unit
		&& unit->getOriginalFaction() == FACTION_PLAYER
		&& (!requirePlayerControl || unit->getFaction() == FACTION_PLAYER)
		&& unit->getGeoscapeSoldier();
}

const ClipSpec *findClip(Event event, SoldierGender gender)
{
	for (const ClipSpec &clip : CLIPS)
	{
		if (clip.event == event && clip.gender == gender)
		{
			return &clip;
		}
	}
	return nullptr;
}

const ClipSpec *pickClip(Event event, SoldierGender gender, int unitId)
{
	std::vector<const ClipSpec *> choices;
	for (const ClipSpec &clip : CLIPS)
	{
		if (clip.event == event && clip.gender == gender)
		{
			choices.push_back(&clip);
		}
	}
	if (choices.empty())
	{
		return nullptr;
	}
	if (choices.size() == 1)
	{
		return choices.front();
	}

	VariantBag &bag = g_state.variantBags[std::make_pair(unitId, event)];
	if (bag.cursor >= bag.order.size())
	{
		bag.order = choices;
		bag.cursor = 0;
		std::uint32_t state = 0x9e3779b9u
			^ (static_cast<std::uint32_t>(unitId) * 0x85ebca6bu)
			^ (static_cast<std::uint32_t>(event) * 0xc2b2ae35u)
			^ (++bag.cycle * 0x27d4eb2du);
		for (std::size_t i = bag.order.size() - 1; i > 0; --i)
		{
			const std::size_t j = nextVariantRandom(state) % (i + 1);
			std::swap(bag.order[i], bag.order[j]);
		}
		if (bag.last && bag.order.front() == bag.last)
		{
			std::swap(bag.order[0], bag.order[1]);
		}
	}

	const ClipSpec *selected = bag.order[bag.cursor++];
	bag.last = selected;
	return selected;
}

int priority(Event event)
{
	switch (event)
	{
		case Event::Death: return 5;
		case Event::FriendlyHit: return 4;
		case Event::AlienSpotted: return 3;
		case Event::Wounded: return 2;
		default: return 1;
	}
}

Uint32 cooldown(Event event)
{
	return event == Event::AlienSpotted ? 10000u : 4000u;
}

void logSuppressed(Event event, const BattleUnit *unit, const char *reason)
{
	EventCounter &counter = g_state.counters.at(static_cast<std::size_t>(event));
	++counter.attempted;
	++counter.suppressed;
	Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventName(event)
		<< " unit=" << (unit ? unit->getId() : -1)
		<< " profile=" << (unit ? genderName(unit->getGender()) : "none")
		<< " result=suppressed reason=" << reason;
}

void releaseChunks()
{
	Mix_HaltChannel(4);
	for (auto &entry : g_state.chunks)
	{
		Mix_FreeChunk(entry.second);
	}
	g_state.chunks.clear();
	g_state.failedLoads.clear();
}

Mix_Chunk *loadClip(const ClipSpec &clip)
{
	const std::string path = std::string(ROOT) + clip.relativePath;
	auto loaded = g_state.chunks.find(path);
	if (loaded != g_state.chunks.end())
	{
		return loaded->second;
	}
	if (g_state.failedLoads.find(path) != g_state.failedLoads.end())
	{
		return nullptr;
	}

	SDL_RWops *rw = em_file_to_rwops(path.c_str());
	if (!rw)
	{
		g_state.failedLoads.insert(path);
		Log(LOG_ERROR) << "[VOICE_G0_5] failed to open " << path;
		return nullptr;
	}
	Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, SDL_TRUE);
	if (!chunk)
	{
		g_state.failedLoads.insert(path);
		Log(LOG_ERROR) << "[VOICE_G0_5] failed to decode " << path
			<< " error=" << Mix_GetError();
		return nullptr;
	}
	g_state.chunks[path] = chunk;
	return chunk;
}

bool submit(Event event, BattleUnit *unit, bool forceInterrupt = false)
{
	if (!g_state.active || !isDiver(unit, event != Event::Death))
	{
		return false;
	}

	if (!findClip(event, unit->getGender()))
	{
		logSuppressed(event, unit, "profile_has_no_pilot_clip");
		return false;
	}

	EventCounter &counter = g_state.counters.at(static_cast<std::size_t>(event));
	++counter.attempted;
	if (Options::mute)
	{
		++counter.suppressed;
		Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventName(event)
			<< " unit=" << unit->getId() << " profile=" << genderName(unit->getGender())
			<< " result=suppressed reason=muted";
		return false;
	}

	const Uint32 now = SDL_GetTicks();
	const std::pair<int, Event> cooldownKey(unit->getId(), event);
	auto last = g_state.lastFired.find(cooldownKey);
	if (!forceInterrupt && last != g_state.lastFired.end() && now - last->second < cooldown(event))
	{
		++counter.suppressed;
		Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventName(event)
			<< " unit=" << unit->getId() << " profile=" << genderName(unit->getGender())
			<< " result=suppressed reason=cooldown";
		return false;
	}

	const int requestedPriority = priority(event);
	if (Mix_Playing(4))
	{
		if (!forceInterrupt && requestedPriority <= g_state.currentPriority)
		{
			++counter.suppressed;
			Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventName(event)
				<< " unit=" << unit->getId() << " profile=" << genderName(unit->getGender())
				<< " result=suppressed reason=channel_busy";
			return false;
		}
		Mix_HaltChannel(4);
	}

	const ClipSpec *clip = pickClip(event, unit->getGender(), unit->getId());
	if (!clip)
	{
		++counter.suppressed;
		return false;
	}
	Mix_Chunk *chunk = loadClip(*clip);
	if (!chunk)
	{
		++counter.suppressed;
		Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventName(event)
			<< " unit=" << unit->getId() << " profile=" << genderName(unit->getGender())
			<< " result=suppressed reason=load_failed";
		return false;
	}

	const int channel = Mix_PlayChannel(4, chunk, 0);
	if (channel != 4)
	{
		++counter.suppressed;
		Log(LOG_WARNING) << "[VOICE_G0_5] event=" << eventName(event)
			<< " unit=" << unit->getId() << " profile=" << genderName(unit->getGender())
			<< " result=suppressed reason=playback_failed error=" << Mix_GetError();
		return false;
	}

	g_state.lastFired[cooldownKey] = now;
	g_state.currentPriority = requestedPriority;
	++counter.fired;
	Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventName(event)
		<< " unit=" << unit->getId() << " profile=" << genderName(unit->getGender())
		<< " result=fired clip=" << clip->relativePath;
	return true;
}

}

void CalypsoVoiceG05::beginMission()
{
	if (g_state.active)
	{
		releaseChunks();
	}
	g_state = PilotState{};
	g_state.active = true;
	Log(LOG_INFO) << "[VOICE_G0_5] development-only pilot active; clips=16 selected_variants=4";
}

void CalypsoVoiceG05::endMission()
{
	if (!g_state.active)
	{
		return;
	}
	for (std::size_t i = 0; i < static_cast<std::size_t>(Event::Count); ++i)
	{
		const EventCounter &counter = g_state.counters.at(i);
		Log(LOG_INFO) << "[VOICE_G0_5_SUMMARY] event=" << EVENT_NAMES.at(i)
			<< " attempted=" << counter.attempted
			<< " fired=" << counter.fired
			<< " suppressed=" << counter.suppressed;
	}
	releaseChunks();
	g_state.active = false;
}

bool CalypsoVoiceG05::handleSelection(BattleUnit *unit, bool sameUnit)
{
	if (!g_state.active || !isDiver(unit) || unit->isOut())
	{
		return false;
	}

	if (!sameUnit)
	{
		g_state.repeatUnitId = unit->getId();
		g_state.repeatClicks = 0;
		g_state.lastRepeatClick = 0;
		submit(Event::Selected, unit);
		return true;
	}

	const Uint32 now = SDL_GetTicks();
	if (g_state.repeatUnitId != unit->getId()
		|| g_state.lastRepeatClick == 0
		|| now - g_state.lastRepeatClick > RESELECT_WINDOW_MS)
	{
		g_state.repeatUnitId = unit->getId();
		g_state.repeatClicks = 1;
	}
	else
	{
		++g_state.repeatClicks;
	}
	g_state.lastRepeatClick = now;

	if (g_state.repeatClicks == 1)
	{
		submit(Event::Reselected, unit);
	}
	else if (g_state.repeatClicks == 3)
	{
		submit(Event::Annoyed1, unit);
	}
	return true;
}

void CalypsoVoiceG05::onAlienSpotted(BattleUnit *spotter, BattleUnit *hostile)
{
	if (!g_state.active || !isDiver(spotter) || !hostile || hostile->getFaction() != FACTION_HOSTILE)
	{
		return;
	}
	const std::pair<int, int> contact(spotter->getId(), hostile->getId());
	if (!g_state.spottedHostiles.insert(contact).second)
	{
		logSuppressed(Event::AlienSpotted, spotter, "hostile_already_reported");
		return;
	}
	submit(Event::AlienSpotted, spotter);
}

void CalypsoVoiceG05::onDamage(BattleUnit *attacker, BattleUnit *target, int healthDamage, int stunDamage)
{
	if (!g_state.active || !target || (healthDamage <= 0 && stunDamage <= 0))
	{
		return;
	}

	const bool playerFriendlyHit = isDiver(attacker)
		&& target->getOriginalFaction() == FACTION_PLAYER;
	if (playerFriendlyHit && findClip(Event::FriendlyHit, attacker->getGender()))
	{
		submit(Event::FriendlyHit, attacker);
		return;
	}

	if (healthDamage > 0
		&& target->getHealth() > 0
		&& isDiver(target)
		&& findClip(Event::Wounded, target->getGender()))
	{
		// Casualty classification happens after TileEngine reports damage. Delay
		// the bark so a lethal hit cannot say both "I'm hit" and "I'm down".
		g_state.pendingWounded[target->getId()] = target;
	}
}

void CalypsoVoiceG05::onCasualtyResolved(BattleUnit *unit)
{
	if (!g_state.active || !unit)
	{
		return;
	}

	auto pending = g_state.pendingWounded.find(unit->getId());
	if (pending == g_state.pendingWounded.end())
	{
		return;
	}
	g_state.pendingWounded.erase(pending);

	if (unit->getHealth() <= 0)
	{
		logSuppressed(Event::Wounded, unit, "fatal_damage");
		return;
	}
	if (unit->isOutThresholdExceed() || unit->isOut())
	{
		logSuppressed(Event::Wounded, unit, "unit_out");
		return;
	}
	submit(Event::Wounded, unit);
}

bool CalypsoVoiceG05::onDeath(BattleUnit *unit)
{
	if (!g_state.active || !isDiver(unit, false) || !findClip(Event::Death, unit->getGender()))
	{
		return false;
	}
	if (g_state.voicedDeaths.find(unit->getId()) != g_state.voicedDeaths.end())
	{
		logSuppressed(Event::Death, unit, "death_already_reported");
		return true;
	}
	if (submit(Event::Death, unit, true))
	{
		g_state.voicedDeaths.insert(unit->getId());
		return true;
	}
	return false;
}

}

extern "C" int calypso_voice_g05_enabled()
{
	return 1;
}

#endif
