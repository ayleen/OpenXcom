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
	Annoyed2,
	Annoyed3,
	MoveAck,
	WeaponReady,
	OutOfAmmo,
	AlienSpotted,
	GrenadeThrow,
	HostileHit,
	Miss,
	HostileKill,
	FriendlyHit,
	CivilianHit,
	Wounded,
	Panic,
	Death,
	Count
};

struct EventSpec
{
	const char *name;
	const char *fileStem;
	unsigned int variants;
	Uint32 cooldownMs;
	int priority;
};

struct EventCounter
{
	unsigned int attempted = 0;
	unsigned int fired = 0;
	unsigned int suppressed = 0;
};

struct VariantBag
{
	std::vector<unsigned int> order;
	std::size_t cursor = 0;
	unsigned int cycle = 0;
	unsigned int last = 0;
};

struct AttackOutcome
{
	BattleUnit *actor = nullptr;
	bool contactedUnit = false;
	bool hostileDamaged = false;
	bool friendlyDamaged = false;
	bool civilianDamaged = false;
	std::set<BattleUnit *> damagedHostiles;
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
	std::map<int, AttackOutcome> attackOutcomes;
	std::map<int, Uint32> selectionFlavorLockedUntil;
	int repeatUnitId = -1;
	unsigned int repeatClicks = 0;
	Uint32 lastRepeatClick = 0;
	int currentPriority = 0;
};

constexpr const char *ROOT = "/game/calypso-voice-g0.5/";
constexpr Uint32 RESELECT_WINDOW_MS = 8000;
constexpr Uint32 FINAL_ANNOYANCE_LOCK_MS = 15000;

constexpr std::array<EventSpec, static_cast<std::size_t>(Event::Count)> EVENT_SPECS = {{
	{"selected", "SELECTED", 4, 1500, 1},
	{"reselected", "RESELECTED", 3, 1500, 1},
	{"annoyed_1", "ANNOYED_1", 2, 4000, 1},
	{"annoyed_2", "ANNOYED_2", 2, 4000, 1},
	{"annoyed_3", "ANNOYED_3", 2, 4000, 1},
	{"move_ack", "MOVE_ACK", 3, 5000, 2},
	{"weapon_ready", "WEAPON_READY", 3, 6000, 2},
	{"out_of_ammo", "OUT_OF_AMMO", 2, 8000, 7},
	{"alien_spotted", "ALIEN_SPOTTED", 4, 10000, 8},
	{"grenade_throw", "GRENADE_THROW", 3, 5000, 5},
	{"hostile_hit", "HOSTILE_HIT", 3, 5000, 4},
	{"miss", "MISS", 3, 8000, 3},
	{"hostile_kill", "HOSTILE_KILL", 4, 3000, 6},
	{"friendly_hit", "FRIENDLY_HIT", 3, 8000, 9},
	{"civilian_hit", "CIVILIAN_HIT", 3, 8000, 9},
	{"wounded", "WOUNDED", 3, 6000, 7},
	{"panic", "PANIC", 3, 12000, 7},
	{"death", "DEATH", 2, 0, 8},
}};

PilotState g_state;

const EventSpec &eventSpec(Event event)
{
	return EVENT_SPECS.at(static_cast<std::size_t>(event));
}

std::uint32_t nextVariantRandom(std::uint32_t &state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

const char *profileName(const BattleUnit *unit)
{
	if (!unit)
	{
		return "none";
	}
	if (unit->getGender() != GENDER_FEMALE)
	{
		return "diver_en_m_custom_v2";
	}

	// The geoscape Soldier ID survives save/load and mission transitions. Use
	// it instead of gameplay RNG so each female Diver keeps one voice while the
	// roster is distributed deterministically between the three pilot profiles.
	const Soldier *soldier = unit->getGeoscapeSoldier();
	const int stableId = soldier ? soldier->getId() : unit->getId();
	switch (static_cast<unsigned int>(stableId) % 3u)
	{
		case 0u: return "diver_en_f_custom_kate";
		case 1u: return "diver_en_f_custom_sasha";
		default: return "diver_en_f_custom_sandra";
	}
}

bool isDiver(const BattleUnit *unit, bool requirePlayerControl = true)
{
	return unit
		&& unit->getOriginalFaction() == FACTION_PLAYER
		&& (!requirePlayerControl || unit->getFaction() == FACTION_PLAYER)
		&& unit->getGeoscapeSoldier();
}

bool timeBefore(Uint32 now, Uint32 deadline)
{
	return static_cast<Sint32>(now - deadline) < 0;
}

unsigned int pickVariant(Event event, int unitId)
{
	const unsigned int variants = eventSpec(event).variants;
	if (variants <= 1)
	{
		return 1;
	}

	VariantBag &bag = g_state.variantBags[std::make_pair(unitId, event)];
	if (bag.cursor >= bag.order.size())
	{
		bag.order.clear();
		for (unsigned int i = 1; i <= variants; ++i)
		{
			bag.order.push_back(i);
		}
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
		if (bag.last != 0 && bag.order.front() == bag.last)
		{
			std::swap(bag.order[0], bag.order[1]);
		}
	}

	const unsigned int selected = bag.order[bag.cursor++];
	bag.last = selected;
	return selected;
}

std::string clipPath(Event event, const BattleUnit *unit, unsigned int variant)
{
	return std::string(profileName(unit))
		+ "/STR_CALYPSO_VOICE_" + eventSpec(event).fileStem + "_"
		+ (variant < 10 ? "0" : "") + std::to_string(variant) + ".wav";
}

void logSuppressed(Event event, const BattleUnit *unit, const char *reason)
{
	EventCounter &counter = g_state.counters.at(static_cast<std::size_t>(event));
	++counter.attempted;
	++counter.suppressed;
	Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventSpec(event).name
		<< " unit=" << (unit ? unit->getId() : -1)
		<< " profile=" << profileName(unit)
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

Mix_Chunk *loadClip(const std::string &relativePath)
{
	const std::string path = std::string(ROOT) + relativePath;
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

	EventCounter &counter = g_state.counters.at(static_cast<std::size_t>(event));
	++counter.attempted;
	if (Options::mute)
	{
		++counter.suppressed;
		Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=muted";
		return false;
	}

	const Uint32 now = SDL_GetTicks();
	const std::pair<int, Event> cooldownKey(unit->getId(), event);
	auto last = g_state.lastFired.find(cooldownKey);
	if (!forceInterrupt && last != g_state.lastFired.end()
		&& now - last->second < eventSpec(event).cooldownMs)
	{
		++counter.suppressed;
		Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=cooldown";
		return false;
	}

	const int requestedPriority = eventSpec(event).priority;
	if (Mix_Playing(4))
	{
		if (!forceInterrupt && requestedPriority <= g_state.currentPriority)
		{
			++counter.suppressed;
			Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventSpec(event).name
				<< " unit=" << unit->getId() << " profile=" << profileName(unit)
				<< " result=suppressed reason=channel_busy";
			return false;
		}
		Mix_HaltChannel(4);
	}

	const unsigned int variant = pickVariant(event, unit->getId());
	const std::string relativePath = clipPath(event, unit, variant);
	Mix_Chunk *chunk = loadClip(relativePath);
	if (!chunk)
	{
		++counter.suppressed;
		Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=load_failed";
		return false;
	}

	const int channel = Mix_PlayChannel(4, chunk, 0);
	if (channel != 4)
	{
		++counter.suppressed;
		Log(LOG_WARNING) << "[VOICE_G0_5] event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=playback_failed error=" << Mix_GetError();
		return false;
	}

	g_state.lastFired[cooldownKey] = now;
	g_state.currentPriority = requestedPriority;
	++counter.fired;
	Log(LOG_INFO) << "[VOICE_G0_5] event=" << eventSpec(event).name
		<< " unit=" << unit->getId() << " profile=" << profileName(unit)
		<< " result=fired clip=" << relativePath;
	return true;
}

bool selectionFlavorLocked(BattleUnit *unit, Uint32 now)
{
	auto locked = g_state.selectionFlavorLockedUntil.find(unit->getId());
	if (locked == g_state.selectionFlavorLockedUntil.end())
	{
		return false;
	}
	if (timeBefore(now, locked->second))
	{
		return true;
	}
	g_state.selectionFlavorLockedUntil.erase(locked);
	return false;
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
	Log(LOG_INFO) << "[VOICE_G0_5] development-only pilot active; clips=208 profiles=4 events=18 shuffle_bags=per_unit_event";
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
		Log(LOG_INFO) << "[VOICE_G0_5_SUMMARY] event=" << EVENT_SPECS.at(i).name
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

	const Uint32 now = SDL_GetTicks();
	if (!sameUnit)
	{
		g_state.repeatUnitId = unit->getId();
		g_state.repeatClicks = 0;
		g_state.lastRepeatClick = 0;
		if (selectionFlavorLocked(unit, now))
		{
			logSuppressed(Event::Selected, unit, "final_annoyance_lockout");
		}
		else
		{
			submit(Event::Selected, unit);
		}
		return true;
	}

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

	if (selectionFlavorLocked(unit, now))
	{
		logSuppressed(Event::Reselected, unit, "final_annoyance_lockout");
		return true;
	}

	if (g_state.repeatClicks == 1)
	{
		submit(Event::Reselected, unit);
	}
	else if (g_state.repeatClicks == 3)
	{
		submit(Event::Annoyed1, unit);
	}
	else if (g_state.repeatClicks == 5)
	{
		submit(Event::Annoyed2, unit);
	}
	else if (g_state.repeatClicks == 7)
	{
		submit(Event::Annoyed3, unit);
		g_state.selectionFlavorLockedUntil[unit->getId()] = now + FINAL_ANNOYANCE_LOCK_MS;
	}
	return true;
}

bool CalypsoVoiceG05::handleMoveOrder(BattleUnit *unit)
{
	if (!g_state.active || !isDiver(unit) || unit->isOut())
	{
		return false;
	}
	submit(Event::MoveAck, unit);
	return true;
}

void CalypsoVoiceG05::onWeaponReady(BattleUnit *unit)
{
	submit(Event::WeaponReady, unit);
}

void CalypsoVoiceG05::onOutOfAmmo(BattleUnit *unit)
{
	submit(Event::OutOfAmmo, unit);
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

void CalypsoVoiceG05::onGrenadeThrown(BattleUnit *unit)
{
	submit(Event::GrenadeThrow, unit);
}

void CalypsoVoiceG05::onAttackStarted(BattleUnit *unit)
{
	if (!g_state.active || !isDiver(unit))
	{
		return;
	}
	AttackOutcome outcome;
	outcome.actor = unit;
	g_state.attackOutcomes[unit->getId()] = outcome;
}

void CalypsoVoiceG05::onDamage(BattleUnit *attacker, BattleUnit *target, int healthDamage, int stunDamage)
{
	if (!g_state.active || !target)
	{
		return;
	}

	if (isDiver(attacker))
	{
		AttackOutcome &outcome = g_state.attackOutcomes[attacker->getId()];
		outcome.actor = attacker;
		outcome.contactedUnit = true;
		if (healthDamage > 0 || stunDamage > 0)
		{
			switch (target->getOriginalFaction())
			{
				case FACTION_HOSTILE:
					outcome.hostileDamaged = true;
					outcome.damagedHostiles.insert(target);
					break;
				case FACTION_PLAYER:
					outcome.friendlyDamaged = true;
					break;
				case FACTION_NEUTRAL:
					outcome.civilianDamaged = true;
					break;
				default:
					break;
			}
		}
	}

	if (healthDamage > 0 && target->getHealth() > 0 && isDiver(target))
	{
		// Casualty classification happens after TileEngine reports damage. Delay
		// the bark so a lethal hit cannot say both "I'm hit" and "I'm down".
		g_state.pendingWounded[target->getId()] = target;
	}
}

void CalypsoVoiceG05::onAttackFinished(BattleUnit *unit)
{
	if (!g_state.active || !unit)
	{
		return;
	}
	auto pending = g_state.attackOutcomes.find(unit->getId());
	if (pending == g_state.attackOutcomes.end())
	{
		return;
	}
	AttackOutcome outcome = pending->second;
	g_state.attackOutcomes.erase(pending);
	if (!isDiver(unit))
	{
		return;
	}

	if (outcome.civilianDamaged)
	{
		submit(Event::CivilianHit, unit);
		return;
	}
	if (outcome.friendlyDamaged)
	{
		submit(Event::FriendlyHit, unit);
		return;
	}
	for (BattleUnit *hostile : outcome.damagedHostiles)
	{
		if (hostile && hostile->getHealth() <= 0)
		{
			submit(Event::HostileKill, unit);
			return;
		}
	}
	if (outcome.hostileDamaged)
	{
		submit(Event::HostileHit, unit);
		return;
	}
	if (!outcome.contactedUnit)
	{
		submit(Event::Miss, unit);
		return;
	}
	logSuppressed(Event::Miss, unit, "armor_blocked");
}

bool CalypsoVoiceG05::onPanic(BattleUnit *unit)
{
	if (!g_state.active || !isDiver(unit))
	{
		return false;
	}
	submit(Event::Panic, unit);
	return true;
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
	if (!g_state.active || !isDiver(unit, false))
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
