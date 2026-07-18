#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_G0_5)

#include "CalypsoVoiceG05.h"
#include "CalypsoVoiceOutcome.h"
#include "CalypsoVoiceSelectionFlavor.h"

#include "../Battlescape/BattlescapeGame.h"
#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"
#include "../Engine/Options.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Soldier.h"
#include "../Mod/RuleItem.h"

#include <SDL.h>
#include <SDL_mixer.h>

#if defined(CALYPSO_VOICE_P_EN)
#include <emscripten.h>
#include "CalypsoVoiceManager.h"
#endif

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
	Flee,
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

#if !defined(CALYPSO_VOICE_P_EN)
struct VariantBag
{
	std::vector<unsigned int> order;
	std::size_t cursor = 0;
	unsigned int cycle = 0;
	unsigned int last = 0;
};
#endif

struct AttackOutcome : CalypsoVoiceOutcomeObservation
{
	unsigned int actionId = 0;
	BattleUnit *actor = nullptr;
};

#if !defined(CALYPSO_VOICE_P_EN)
struct CachedChunk
{
	Mix_Chunk *chunk = nullptr;
	std::uint64_t lastUsed = 0;
};
#endif

struct PilotState
{
	bool active = false;
#if defined(CALYPSO_VOICE_P_EN)
	std::set<std::string> requestedPacks;
#else
	bool packReady = false;
#endif
	unsigned int missionEpoch = 0;
#if !defined(CALYPSO_VOICE_P_EN)
	std::map<std::string, CachedChunk> chunks;
	std::size_t decodedBytes = 0;
	std::uint64_t cacheClock = 0;
#endif
	std::uint32_t cosmeticState = 0x6d2b79f5u;
#if !defined(CALYPSO_VOICE_P_EN)
	std::set<std::string> failedLoads;
#endif
	std::array<EventCounter, static_cast<std::size_t>(Event::Count)> counters{};
	std::map<std::pair<int, Event>, Uint32> lastFired;
	std::set<std::pair<int, int>> spottedHostiles;
	bool hasLastAlienSpottedSubmit = false;
	Uint32 lastAlienSpottedSubmit = 0;
	std::set<int> voicedDeaths;
	std::map<int, BattleUnit *> pendingWounded;
#if !defined(CALYPSO_VOICE_P_EN)
	std::map<std::pair<int, Event>, VariantBag> variantBags;
#endif
	std::map<unsigned int, AttackOutcome> attackOutcomes;
	unsigned int nextActionId = 0;
	std::map<int, Uint32> selectionFlavorLockedUntil;
	CalypsoVoiceSelectionState selectionState;
#if !defined(CALYPSO_VOICE_P_EN)
	int currentPriority = 0;
#endif
};

#if defined(CALYPSO_VOICE_P_EN)
constexpr const char *LOG_TAG = "[VOICE_P_EN]";
#else
constexpr const char *ROOT = "/game/calypso-voice-g0.5/";
constexpr const char *EXTENSION = ".wav";
constexpr const char *LOG_TAG = "[VOICE_G0_5]";
constexpr std::size_t DECODED_CACHE_LIMIT = 4u * 1024u * 1024u;
#endif
constexpr Uint32 FINAL_ANNOYANCE_LOCK_MS = 15000;
constexpr Uint32 SIMULTANEOUS_CONTACT_WINDOW_MS = 800;

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
	{"flee", "FLEE", 3, 8000, 7},
}};

PilotState g_state;
unsigned int g_nextMissionEpoch = 0;
#if defined(CALYPSO_VOICE_P_EN)
CalypsoVoiceManager g_manager;
#endif

const EventSpec &eventSpec(Event event)
{
	return EVENT_SPECS.at(static_cast<std::size_t>(event));
}

unsigned int eventVariants(Event event, const BattleUnit *unit)
{
#if defined(CALYPSO_VOICE_P_EN)
	return static_cast<unsigned int>(g_manager.lineCount(unit, eventSpec(event).name));
#else
	(void)unit;
	return eventSpec(event).variants;
#endif
}

Uint32 eventCooldown(Event event, const BattleUnit *unit)
{
#if defined(CALYPSO_VOICE_P_EN)
	return static_cast<Uint32>(g_manager.cooldownMs(unit, eventSpec(event).name));
#else
	(void)unit;
	return eventSpec(event).cooldownMs;
#endif
}

int eventPriority(Event event, const BattleUnit *unit)
{
#if defined(CALYPSO_VOICE_P_EN)
	return g_manager.priority(unit, eventSpec(event).name);
#else
	(void)unit;
	return eventSpec(event).priority;
#endif
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
	const Soldier *soldier = unit->getGeoscapeSoldier();
#if defined(CALYPSO_VOICE_P_EN)
	return !unit->getVoiceProfile().empty()
		? unit->getVoiceProfile().c_str()
		: "none";
#else
	if (unit->getGender() != GENDER_FEMALE)
	{
		return "diver_en_m_custom_v2";
	}

	// The geoscape Soldier ID survives save/load and mission transitions. Use
	// it instead of gameplay RNG so each female Diver keeps one voice while the
	// roster is distributed deterministically between the three pilot profiles.
	const int stableId = soldier ? soldier->getId() : unit->getId();
	switch (static_cast<unsigned int>(stableId) % 3u)
	{
		case 0u: return "diver_en_f_custom_kate";
		case 1u: return "diver_en_f_custom_sasha";
		default: return "diver_en_f_custom_sandra";
	}
#endif
}

bool isDiver(const BattleUnit *unit, bool requirePlayerControl = true)
{
	return unit
		&& unit->getOriginalFaction() == FACTION_PLAYER
		&& (!requirePlayerControl || unit->getFaction() == FACTION_PLAYER)
		&& unit->getGeoscapeSoldier();
}

bool isCivilianUnit(const BattleUnit *unit)
{
	return unit
		&& unit->getFaction() == FACTION_NEUTRAL
		&& unit->getOriginalFaction() == FACTION_NEUTRAL;
}

/// Catalogue of events a civilian profile may own (Phase 44 E3). Diver
/// command/action events remain diver-only and are never submitted for a
/// civilian unit.
bool isCivilianEvent(Event event)
{
	return event == Event::AlienSpotted || event == Event::Wounded
		|| event == Event::Panic || event == Event::Death
		|| event == Event::Flee;
}

/// True when a civilian unit is allowed to submit `event` under the active
/// configuration. In P_EN this additionally requires the unit's production
/// RuleVoiceProfile to declare the semantic event (so missing civilian
/// profiles/events keep stock behavior and the system never claims handling).
/// The non-P_EN development corpus has no civilian profiles, so civilians are
/// never eligible there and the pilot spike is byte-for-byte unchanged.
bool civilianMaySubmit(const BattleUnit *unit, Event event)
{
	if (!isCivilianUnit(unit) || !isCivilianEvent(event))
	{
		return false;
	}
#if defined(CALYPSO_VOICE_P_EN)
	return g_manager.hasEvent(unit, eventSpec(event).name);
#else
	return false;
#endif
}

bool timeBefore(Uint32 now, Uint32 deadline)
{
	return static_cast<Sint32>(now - deadline) < 0;
}

unsigned int pickVariant(Event event, const BattleUnit *unit)
{
#if defined(CALYPSO_VOICE_P_EN)
	const std::size_t selected = g_manager.selectLine(unit, eventSpec(event).name,
		static_cast<std::size_t>(event));
	return selected == CalypsoVoiceLineBag::noLine()
		? 0u : static_cast<unsigned int>(selected + 1u);
#else
	const unsigned int variants = eventVariants(event, unit);
	if (variants == 0)
	{
		return 0;
	}
	if (variants <= 1)
	{
		return 1;
	}

	const int unitId = unit->getId();
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
#endif
}

std::string clipPath(Event event, const BattleUnit *unit, unsigned int variant)
{
#if defined(CALYPSO_VOICE_P_EN)
	return variant == 0 ? std::string()
		: g_manager.clipPath(unit, eventSpec(event).name, variant - 1);
#else
	return std::string(profileName(unit))
		+ "/STR_CALYPSO_VOICE_" + eventSpec(event).fileStem + "_"
		+ (variant < 10 ? "0" : "") + std::to_string(variant) + EXTENSION;
#endif
}

void logSuppressed(Event event, const BattleUnit *unit, const char *reason)
{
	EventCounter &counter = g_state.counters.at(static_cast<std::size_t>(event));
	++counter.attempted;
	++counter.suppressed;
	Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
		<< " unit=" << (unit ? unit->getId() : -1)
		<< " profile=" << profileName(unit)
		<< " result=suppressed reason=" << reason;
}

#if defined(CALYPSO_VOICE_P_EN)
bool isFlavorEvent(Event event)
{
	return event == Event::Selected || event == Event::Reselected
		|| event == Event::Annoyed1 || event == Event::Annoyed2
		|| event == Event::Annoyed3 || event == Event::MoveAck
		|| event == Event::WeaponReady;
}

bool isSafetyEvent(Event event)
{
	return event == Event::FriendlyHit || event == Event::CivilianHit;
}

bool findEvent(const std::string &name, Event &event)
{
	for (std::size_t i = 0; i < static_cast<std::size_t>(Event::Count); ++i)
	{
		if (name == EVENT_SPECS.at(i).name)
		{
			event = static_cast<Event>(i);
			return true;
		}
	}
	return false;
}

void recordManagerResult(Event event, BattleUnit *unit,
	const CalypsoVoiceRequestResult &result)
{
	EventCounter &counter = g_state.counters.at(static_cast<std::size_t>(event));
	if (result.displacedUnit && !result.displacedEvent.empty())
	{
		Event displaced;
		if (findEvent(result.displacedEvent, displaced))
		{
			EventCounter &displacedCounter = g_state.counters.at(
				static_cast<std::size_t>(displaced));
			++displacedCounter.suppressed;
			Log(LOG_INFO) << LOG_TAG << " event=" << result.displacedEvent
				<< " unit=" << result.displacedUnit->getId()
				<< " profile=" << profileName(result.displacedUnit)
				<< " result=suppressed reason=queue_replaced";
		}
	}

	switch (result.status)
	{
		case CalypsoVoiceRequestStatus::Played:
			++counter.fired;
			Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
				<< " unit=" << (unit ? unit->getId() : -1)
				<< " profile=" << profileName(unit)
				<< " result=fired clip=" << result.clipPath;
			break;
		case CalypsoVoiceRequestStatus::SubtitleOnly:
			++counter.fired;
			Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
				<< " unit=" << (unit ? unit->getId() : -1)
				<< " profile=" << profileName(unit)
				<< " result=subtitle_only line=" << result.lineId
				<< " reason=" << result.reason;
			break;
		case CalypsoVoiceRequestStatus::Queued:
			Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
				<< " unit=" << (unit ? unit->getId() : -1)
				<< " profile=" << profileName(unit)
				<< " result=queued reason=" << result.reason;
			break;
		case CalypsoVoiceRequestStatus::Suppressed:
		case CalypsoVoiceRequestStatus::LoadFailed:
		case CalypsoVoiceRequestStatus::PlaybackFailed:
			++counter.suppressed;
			Log(result.status == CalypsoVoiceRequestStatus::PlaybackFailed
				? LOG_WARNING : LOG_INFO)
				<< LOG_TAG << " event=" << eventSpec(event).name
				<< " unit=" << (unit ? unit->getId() : -1)
				<< " profile=" << profileName(unit)
				<< " result=suppressed reason=" << result.reason
				<< (result.status == CalypsoVoiceRequestStatus::PlaybackFailed
					? std::string(" error=") + Mix_GetError() : std::string());
			break;
		case CalypsoVoiceRequestStatus::Idle:
		default:
			break;
	}
}
#endif

#if !defined(CALYPSO_VOICE_P_EN)
void releaseChunks()
{
	Mix_HaltChannel(4);
	for (auto &entry : g_state.chunks)
	{
		Mix_FreeChunk(entry.second.chunk);
	}
	g_state.chunks.clear();
	g_state.decodedBytes = 0;
	g_state.failedLoads.clear();
}
#endif

#if !defined(CALYPSO_VOICE_P_EN)
bool makeCacheRoom(std::size_t requiredBytes)
{
	while (g_state.decodedBytes + requiredBytes > DECODED_CACHE_LIMIT)
	{
		Mix_Chunk *playing = Mix_Playing(4) ? Mix_GetChunk(4) : nullptr;
		auto oldest = g_state.chunks.end();
		for (auto it = g_state.chunks.begin(); it != g_state.chunks.end(); ++it)
		{
			if (it->second.chunk == playing)
			{
				continue;
			}
			if (oldest == g_state.chunks.end()
				|| it->second.lastUsed < oldest->second.lastUsed)
			{
				oldest = it;
			}
		}
		if (oldest == g_state.chunks.end())
		{
			return false;
		}
		g_state.decodedBytes -= oldest->second.chunk->alen;
		Mix_FreeChunk(oldest->second.chunk);
		g_state.chunks.erase(oldest);
	}
	return true;
}

Mix_Chunk *loadClip(const std::string &relativePath)
{
	const std::string path = std::string(ROOT) + relativePath;
	auto loaded = g_state.chunks.find(path);
	if (loaded != g_state.chunks.end())
	{
		loaded->second.lastUsed = ++g_state.cacheClock;
		return loaded->second.chunk;
	}
	if (g_state.failedLoads.find(path) != g_state.failedLoads.end())
	{
		return nullptr;
	}

	SDL_RWops *rw = em_file_to_rwops(path.c_str());
	if (!rw)
	{
		g_state.failedLoads.insert(path);
		Log(LOG_ERROR) << LOG_TAG << " failed to open " << path;
		return nullptr;
	}
	Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, SDL_TRUE);
	if (!chunk)
	{
		g_state.failedLoads.insert(path);
		Log(LOG_ERROR) << LOG_TAG << " failed to decode " << path
			<< " error=" << Mix_GetError();
		return nullptr;
	}
	if (!makeCacheRoom(chunk->alen))
	{
		Log(LOG_WARNING) << LOG_TAG << " decoded cache limit rejected " << path
			<< " bytes=" << chunk->alen;
		Mix_FreeChunk(chunk);
		return nullptr;
	}
	g_state.decodedBytes += chunk->alen;
	g_state.chunks[path] = CachedChunk{chunk, ++g_state.cacheClock};
	return chunk;
}
#endif

bool submit(Event event, BattleUnit *unit, bool forceInterrupt = false,
	bool *handledByVoice = nullptr)
{
	if (handledByVoice)
	{
		*handledByVoice = false;
	}
	if (!g_state.active)
	{
		return false;
	}
	// Diver command/action events stay diver-only and player-controlled. A
	// civilian unit may submit only its own catalogue events, and only when
	// its production profile declares them (handled by civilianMaySubmit).
	const bool eligible = isDiver(unit, event != Event::Death)
		|| civilianMaySubmit(unit, event);
	if (!eligible)
	{
		return false;
	}
#if defined(CALYPSO_VOICE_P_EN)
	if (!Options::calypsoVoicesEnabled)
	{
		return false;
	}
#endif
#if !defined(CALYPSO_VOICE_P_EN)
	if (!g_state.packReady)
	{
		return false;
	}
	if (handledByVoice)
	{
		// Preserve the accepted G0.5 behavior: once its verified pack owns a
		// command event, deliberate cooldown/mute suppression must not fall
		// through to a second, stock unit response.
		*handledByVoice = true;
	}
#endif

	EventCounter &counter = g_state.counters.at(static_cast<std::size_t>(event));
	++counter.attempted;
#if defined(CALYPSO_VOICE_P_EN)
	if (Options::mute)
	{
		if (handledByVoice)
		{
			*handledByVoice = true;
		}
		++counter.suppressed;
		Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=muted";
		return false;
	}
	const CalypsoVoiceRequestResult result = g_manager.submit(unit,
		eventSpec(event).name, SDL_GetTicks(), isFlavorEvent(event),
		isSafetyEvent(event), forceInterrupt);
	recordManagerResult(event, unit, result);
	if (handledByVoice)
	{
		*handledByVoice = result.status == CalypsoVoiceRequestStatus::Played
			|| result.status == CalypsoVoiceRequestStatus::Queued
			|| (result.status == CalypsoVoiceRequestStatus::Suppressed
				&& result.reason != "missing_ruleset_event");
	}
	return result.status == CalypsoVoiceRequestStatus::Played;
#else
	if (Options::mute)
	{
		++counter.suppressed;
		Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=muted";
		return false;
	}

	const Uint32 now = SDL_GetTicks();
	const std::pair<int, Event> cooldownKey(unit->getId(), event);
	auto last = g_state.lastFired.find(cooldownKey);
	if (!forceInterrupt && last != g_state.lastFired.end()
		&& now - last->second < eventCooldown(event, unit))
	{
		++counter.suppressed;
		Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=cooldown";
		return false;
	}

	const int requestedPriority = eventPriority(event, unit);
	if (Mix_Playing(4))
	{
		if (!forceInterrupt && requestedPriority <= g_state.currentPriority)
		{
			++counter.suppressed;
			Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
				<< " unit=" << unit->getId() << " profile=" << profileName(unit)
				<< " result=suppressed reason=channel_busy";
			return false;
		}
		Mix_HaltChannel(4);
	}

	const unsigned int variant = pickVariant(event, unit);
	const std::string relativePath = clipPath(event, unit, variant);
	Mix_Chunk *chunk = loadClip(relativePath);
	if (!chunk)
	{
		++counter.suppressed;
		Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=load_failed";
		return false;
	}
	const int channel = Mix_PlayChannel(4, chunk, 0);
	if (channel != 4)
	{
		++counter.suppressed;
		Log(LOG_WARNING) << LOG_TAG << " event=" << eventSpec(event).name
			<< " unit=" << unit->getId() << " profile=" << profileName(unit)
			<< " result=suppressed reason=playback_failed error=" << Mix_GetError();
		return false;
	}

	g_state.lastFired[cooldownKey] = now;
	g_state.currentPriority = requestedPriority;
	++counter.fired;
	Log(LOG_INFO) << LOG_TAG << " event=" << eventSpec(event).name
		<< " unit=" << unit->getId() << " profile=" << profileName(unit)
		<< " result=fired clip=" << relativePath;
	if (handledByVoice)
	{
		*handledByVoice = true;
	}
	return true;
#endif
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

void CalypsoVoiceG05::beginMission(SavedBattleGame *save)
{
	if (g_state.active)
	{
#if defined(CALYPSO_VOICE_P_EN)
		g_manager.endMission();
#else
		releaseChunks();
#endif
	}
	g_state = PilotState{};
	g_state.active = true;
	g_state.missionEpoch = ++g_nextMissionEpoch;
	g_state.cosmeticState ^= g_state.missionEpoch * 0x9e3779b9u;
#if defined(CALYPSO_VOICE_P_EN)
	g_manager.beginMission(save->getMod(), g_state.missionEpoch,
		save->getDepth() > 0);
	g_state.requestedPacks = g_manager.requiredPacks(*save->getUnits());
	for (const std::string &pack : g_state.requestedPacks)
	{
		Log(LOG_INFO) << LOG_TAG << " requesting lazy pack=" << pack
			<< " epoch=" << g_state.missionEpoch;
		EM_ASM({
			if (globalThis.calypsoVoicePacks?.request) {
				globalThis.calypsoVoicePacks.request(UTF8ToString($0), $1);
			}
		}, pack.c_str(), g_state.missionEpoch);
	}
#else
	g_state.packReady = true;
	Log(LOG_INFO) << LOG_TAG << " development-only pilot active; clips=208 profiles=4 events=18 shuffle_bags=per_unit_event";
#endif
}

void CalypsoVoiceG05::think()
{
#if defined(CALYPSO_VOICE_P_EN)
	if (!g_state.active)
	{
		return;
	}
	const CalypsoVoiceRequestResult result = g_manager.update(
		SDL_GetTicks(), !Options::mute && Options::calypsoVoicesEnabled);
	if (result.status == CalypsoVoiceRequestStatus::Idle)
	{
		return;
	}
	Event event;
	if (findEvent(result.event, event))
	{
		recordManagerResult(event, result.unit, result);
	}
#endif
}

CalypsoVoiceSubtitleSnapshot CalypsoVoiceG05::subtitle(unsigned int nowMs)
{
	CalypsoVoiceSubtitleSnapshot result;
#if defined(CALYPSO_VOICE_P_EN)
	const CalypsoVoiceSubtitleState state = g_manager.subtitle(nowMs);
	result.active = state.active;
	result.tactical = state.tactical;
	result.unit = state.unit;
	result.lineId = state.lineId;
#else
	(void)nowMs;
#endif
	return result;
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
#if defined(CALYPSO_VOICE_P_EN)
		Log(LOG_INFO) << LOG_TAG << " summary event=" << EVENT_SPECS.at(i).name
#else
		Log(LOG_INFO) << "[VOICE_G0_5_SUMMARY] event=" << EVENT_SPECS.at(i).name
#endif
			<< " attempted=" << counter.attempted
			<< " fired=" << counter.fired
			<< " suppressed=" << counter.suppressed;
	}
	g_state.active = false;
#if defined(CALYPSO_VOICE_P_EN)
	g_manager.endMission();
	for (const std::string &pack : g_state.requestedPacks)
	{
		EM_ASM({
			globalThis.calypsoVoicePacks?.release(UTF8ToString($0));
		}, pack.c_str());
	}
#else
	releaseChunks();
#endif
}

bool CalypsoVoiceG05::onPackResult(const std::string &pack,
	unsigned int missionEpoch, bool available)
{
#if defined(CALYPSO_VOICE_P_EN)
	if (!g_state.active || g_state.missionEpoch != missionEpoch
		|| g_state.requestedPacks.find(pack) == g_state.requestedPacks.end())
	{
		return false;
	}
	g_manager.setPackAvailable(pack, available);
	Log(available ? LOG_INFO : LOG_WARNING) << LOG_TAG
		<< " pack=" << pack
		<< " result=" << (available ? "ready" : "unavailable")
		<< " epoch=" << missionEpoch;
	return available;
#else
	(void)pack;
	(void)missionEpoch;
	(void)available;
	return false;
#endif
}

bool CalypsoVoiceG05::handleSelection(BattleUnit *unit, bool sameUnit)
{
	if (!g_state.active || !isDiver(unit) || unit->isOut())
	{
		return false;
	}
#if defined(CALYPSO_VOICE_P_EN)
	if (!Options::calypsoVoicesEnabled)
	{
		return false;
	}
#endif
#if !defined(CALYPSO_VOICE_P_EN)
	if (!g_state.packReady)
	{
		return false;
	}
#endif

	const Uint32 now = SDL_GetTicks();
	const bool locked = selectionFlavorLocked(unit, now);
	const CalypsoVoiceSelectionFlavor flavor = calypsoAdvanceVoiceSelection(
		g_state.selectionState, unit->getId(), sameUnit, now, locked);
	if (locked)
	{
		logSuppressed(sameUnit ? Event::Reselected : Event::Selected, unit,
			"final_annoyance_lockout");
		return true;
	}

	Event event = Event::Reselected;
	switch (flavor)
	{
		case CalypsoVoiceSelectionFlavor::Selected: event = Event::Selected; break;
		case CalypsoVoiceSelectionFlavor::Reselected: event = Event::Reselected; break;
		case CalypsoVoiceSelectionFlavor::Annoyed1: event = Event::Annoyed1; break;
		case CalypsoVoiceSelectionFlavor::Annoyed2: event = Event::Annoyed2; break;
		case CalypsoVoiceSelectionFlavor::Annoyed3:
			event = Event::Annoyed3;
			g_state.selectionFlavorLockedUntil[unit->getId()] =
				now + FINAL_ANNOYANCE_LOCK_MS;
			break;
		case CalypsoVoiceSelectionFlavor::None:
		default:
			return true;
	}
	bool handled = false;
	submit(event, unit, false, &handled);
	return handled;
}

bool CalypsoVoiceG05::handleMoveOrder(BattleUnit *unit)
{
	if (!g_state.active || !isDiver(unit) || unit->isOut())
	{
		return false;
	}
#if defined(CALYPSO_VOICE_P_EN)
	if (!Options::calypsoVoicesEnabled)
	{
		return false;
	}
#endif
#if !defined(CALYPSO_VOICE_P_EN)
	if (!g_state.packReady)
	{
		return false;
	}
#endif
	bool handled = false;
	submit(Event::MoveAck, unit, false, &handled);
	return handled;
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
	if (!g_state.active || !hostile || hostile->getFaction() != FACTION_HOSTILE
		|| (!isDiver(spotter) && !isCivilianUnit(spotter)))
	{
		return;
	}
	const std::pair<int, int> contact(spotter->getId(), hostile->getId());
	if (!g_state.spottedHostiles.insert(contact).second)
	{
		logSuppressed(Event::AlienSpotted, spotter, "hostile_already_reported");
		return;
	}
	const Uint32 now = SDL_GetTicks();
	if (g_state.hasLastAlienSpottedSubmit
		&& now - g_state.lastAlienSpottedSubmit < SIMULTANEOUS_CONTACT_WINDOW_MS)
	{
		logSuppressed(Event::AlienSpotted, spotter, "simultaneous_contact");
		return;
	}
	g_state.hasLastAlienSpottedSubmit = true;
	g_state.lastAlienSpottedSubmit = now;
	submit(Event::AlienSpotted, spotter);
}

void CalypsoVoiceG05::onGrenadeThrown(BattleUnit *unit)
{
	submit(Event::GrenadeThrow, unit);
}

void CalypsoVoiceG05::onAttackStarted(BattleAction &action)
{
	BattleUnit *unit = action.actor;
	if (!g_state.active || !isDiver(unit))
	{
		return;
	}
	if (action.voiceActionId == 0)
	{
		do
		{
			action.voiceActionId = ++g_state.nextActionId;
		}
		while (action.voiceActionId == 0);
	}
	AttackOutcome outcome;
	outcome.actionId = action.voiceActionId;
	outcome.actor = unit;
	g_state.attackOutcomes[action.voiceActionId] = outcome;
}

void CalypsoVoiceG05::onDamage(const BattleActionAttack &attack,
	BattleUnit *target, int healthDamage, int stunDamage)
{
	if (!g_state.active || !target)
	{
		return;
	}

	BattleUnit *attacker = attack.attacker;
	auto pending = g_state.attackOutcomes.find(attack.voiceActionId);
	if (attack.voiceActionId != 0 && pending != g_state.attackOutcomes.end()
		&& pending->second.actor == attacker && isDiver(attacker))
	{
		AttackOutcome &outcome = pending->second;
		outcome.contactedUnit = true;
		if (healthDamage > 0 || stunDamage > 0)
		{
			switch (target->getOriginalFaction())
			{
				case FACTION_HOSTILE:
					outcome.hostileDamaged = true;
					break;
				case FACTION_PLAYER:
					outcome.friendlyDamagedOrKilled = true;
					break;
				case FACTION_NEUTRAL:
					outcome.civilianDamagedOrKilled = true;
					break;
				default:
					break;
			}
		}
	}

	if (healthDamage > 0 && target->getHealth() > 0
		&& (isDiver(target) || isCivilianUnit(target)))
	{
		// Casualty classification happens after TileEngine reports damage. Delay
		// the bark so a lethal hit cannot say both "I'm hit" and "I'm down".
		g_state.pendingWounded[target->getId()] = target;
	}
}

void CalypsoVoiceG05::onKill(const BattleActionAttack &attack,
	BattleUnit *victim, BattleUnit *creditedKiller)
{
	if (!g_state.active || attack.voiceActionId == 0 || !victim
		|| !creditedKiller)
	{
		return;
	}
	auto pending = g_state.attackOutcomes.find(attack.voiceActionId);
	if (pending == g_state.attackOutcomes.end()
		|| pending->second.actor != creditedKiller)
	{
		return;
	}

	AttackOutcome &outcome = pending->second;
	outcome.contactedUnit = true;
	switch (victim->getOriginalFaction())
	{
		case FACTION_HOSTILE:
			outcome.hostileKilled = true;
			break;
		case FACTION_PLAYER:
			outcome.friendlyDamagedOrKilled = true;
			break;
		case FACTION_NEUTRAL:
			outcome.civilianDamagedOrKilled = true;
			break;
		default:
			break;
	}
}

void CalypsoVoiceG05::onAttackFinished(const BattleAction &action)
{
	BattleUnit *unit = action.actor;
	if (!g_state.active || !unit)
	{
		return;
	}
	auto pending = g_state.attackOutcomes.find(action.voiceActionId);
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

	switch (calypsoDecideVoiceOutcome(outcome))
	{
		case CalypsoVoiceOutcome::CivilianHit:
			submit(Event::CivilianHit, unit);
			break;
		case CalypsoVoiceOutcome::FriendlyHit:
			submit(Event::FriendlyHit, unit);
			break;
		case CalypsoVoiceOutcome::HostileKill:
			submit(Event::HostileKill, unit);
			break;
		case CalypsoVoiceOutcome::HostileHit:
			submit(Event::HostileHit, unit);
			break;
		case CalypsoVoiceOutcome::Miss:
			submit(Event::Miss, unit);
			break;
		case CalypsoVoiceOutcome::Silence:
		default:
			logSuppressed(Event::Miss, unit, "armor_blocked");
			break;
	}
}

bool CalypsoVoiceG05::onPanic(BattleUnit *unit)
{
	if (!g_state.active || (!isDiver(unit) && !isCivilianUnit(unit)))
	{
		return false;
	}
#if defined(CALYPSO_VOICE_P_EN)
	if (!Options::calypsoVoicesEnabled)
	{
		return false;
	}
#endif
	bool handled = false;
	submit(Event::Panic, unit, false, &handled);
	return handled;
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
	if (!g_state.active || (!isDiver(unit, false) && !isCivilianUnit(unit)))
	{
		return false;
	}
#if defined(CALYPSO_VOICE_P_EN)
	if (!Options::calypsoVoicesEnabled)
	{
		return false;
	}
#endif
#if !defined(CALYPSO_VOICE_P_EN)
	if (!g_state.packReady)
	{
		return false;
	}
#endif
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

void CalypsoVoiceG05::onCivilianFlee(BattleUnit *unit)
{
	if (!g_state.active || !isCivilianUnit(unit))
	{
		return;
	}
#if defined(CALYPSO_VOICE_P_EN)
	// submit() enforces the civilian catalogue + profile-declares-flee gates,
	// so a no-op here for guards/profiles lacking "flee".
	submit(Event::Flee, unit);
#else
	// The development G0.5 corpus has no civilian profiles; ignore the event so
	// the pilot spike keeps its existing behavior.
	(void)unit;
#endif
}

}

extern "C" int calypso_voice_g05_enabled()
{
#if defined(CALYPSO_VOICE_P_EN)
	return 0;
#else
	return 1;
#endif
}

extern "C" int calypso_voice_pack_result(const char *pack,
	unsigned int missionEpoch, int available)
{
	return OpenXcom::CalypsoVoiceG05::onPackResult(
		pack ? pack : "", missionEpoch, available != 0) ? 1 : 0;
}

#endif
