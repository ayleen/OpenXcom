#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_P_EN)

#include "CalypsoVoiceManager.h"

#include "RuleVoiceProfile.h"
#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"
#include "../Mod/Mod.h"
#include "../Savegame/BattleUnit.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <algorithm>
#include <array>

namespace OpenXcom
{
namespace
{

constexpr const char *VOICE_ROOT = "/game/voice-packs/";
constexpr const char *LOG_TAG = "[VOICE_P_EN]";
constexpr std::size_t DECODED_CACHE_LIMIT = 4u * 1024u * 1024u;
constexpr std::array<const char *, 19> EVENT_NAMES = {{
	"selected", "reselected", "annoyed_1", "annoyed_2", "annoyed_3",
	"move_ack", "weapon_ready", "out_of_ammo", "alien_spotted",
	"grenade_throw", "hostile_hit", "miss", "hostile_kill", "friendly_hit",
	"civilian_hit", "wounded", "panic", "death", "flee",
}};

const RuleVoiceProfile *resolveProfile(const Mod *mod, const BattleUnit *unit)
{
	return mod && unit ? mod->getVoiceProfile(unit->getVoiceProfile()) : nullptr;
}

const RuleVoiceProfile *resolveAudioProfile(const Mod *mod,
	const BattleUnit *unit, const std::set<std::string> &availablePacks)
{
	const RuleVoiceProfile *profile = resolveProfile(mod, unit);
	if (!profile)
	{
		return nullptr;
	}
	if (availablePacks.find(profile->getPack()) != availablePacks.end())
	{
		return profile;
	}
	if (!profile->getFallbackProfile().empty())
	{
		const RuleVoiceProfile *fallback = mod->getVoiceProfile(
			profile->getFallbackProfile());
		if (fallback
			&& availablePacks.find(fallback->getPack()) != availablePacks.end())
		{
			return fallback;
		}
	}
	return nullptr;
}

const VoiceEventRule *resolveEvent(const Mod *mod, const BattleUnit *unit,
	const std::string &event)
{
	const RuleVoiceProfile *profile = resolveProfile(mod, unit);
	return profile ? profile->getEvent(event) : nullptr;
}

void replaceToken(std::string &path, const std::string &token,
	const std::string &value)
{
	const std::string::size_type position = path.find(token);
	if (position != std::string::npos)
	{
		path.replace(position, token.size(), value);
	}
}

std::string buildClipPath(const RuleVoiceProfile *profile,
	const std::string &event, std::size_t lineIndex, bool underwater)
{
	const VoiceEventRule *rule = profile ? profile->getEvent(event) : nullptr;
	if (!profile || !rule || lineIndex >= rule->lines.size())
	{
		return std::string();
	}
	std::string path = underwater
		? profile->getWetPathTemplate()
		: profile->getDryPathTemplate();
	if (path.empty())
	{
		return std::string();
	}
	replaceToken(path, "{profile}", profile->getId());
	replaceToken(path, "{line}", rule->lines[lineIndex]);
	return profile->getPack() + "/" + path;
}

std::size_t eventSeedIndex(const std::string &event)
{
	for (std::size_t i = 0; i < EVENT_NAMES.size(); ++i)
	{
		if (event == EVENT_NAMES[i])
		{
			return i;
		}
	}
	return 0;
}

}

void CalypsoVoiceManager::beginMission(const Mod *mod, unsigned int missionEpoch,
	bool underwater)
{
	releaseAudio();
	_mod = mod;
	_underwater = underwater;
	_availablePacks.clear();
	_lineBags.clear();
	_lastSpeaker.clear();
	_lastEvent.clear();
	_pending = PendingEvent{};
	_hasLastGlobal = false;
	_lastGlobal = 0;
	_subtitleUnit = nullptr;
	_subtitleLineId.clear();
	_subtitleTactical = false;
	_subtitleStartedMs = 0;
	_subtitleDurationMs = 0;
	_cosmeticState = 0x6d2b79f5u ^ missionEpoch * 0x9e3779b9u;
}

void CalypsoVoiceManager::endMission()
{
	releaseAudio();
	_mod = nullptr;
	_underwater = false;
	_availablePacks.clear();
	_lineBags.clear();
	_lastSpeaker.clear();
	_lastEvent.clear();
	_pending = PendingEvent{};
	_hasLastGlobal = false;
	_lastGlobal = 0;
	_subtitleUnit = nullptr;
	_subtitleLineId.clear();
	_subtitleTactical = false;
	_subtitleStartedMs = 0;
	_subtitleDurationMs = 0;
}

std::set<std::string> CalypsoVoiceManager::requiredPacks(
	const std::vector<BattleUnit *> &units) const
{
	std::set<std::string> packs;
	for (const BattleUnit *unit : units)
	{
		const RuleVoiceProfile *profile = resolveProfile(_mod, unit);
		if (!profile)
		{
			continue;
		}
		packs.insert(profile->getPack());
		if (!profile->getFallbackProfile().empty())
		{
			const RuleVoiceProfile *fallback = _mod->getVoiceProfile(
				profile->getFallbackProfile());
			if (fallback)
			{
				packs.insert(fallback->getPack());
			}
		}
	}
	return packs;
}

void CalypsoVoiceManager::setPackAvailable(const std::string &pack,
	bool available)
{
	if (available)
	{
		_availablePacks.insert(pack);
	}
	else
	{
		_availablePacks.erase(pack);
	}
}

bool CalypsoVoiceManager::audioAvailable(const BattleUnit *unit) const
{
	return resolveAudioProfile(_mod, unit, _availablePacks) != nullptr;
}

bool CalypsoVoiceManager::hasEvent(const BattleUnit *unit,
	const std::string &event) const
{
	return resolveEvent(_mod, unit, event) != nullptr;
}

std::size_t CalypsoVoiceManager::lineCount(const BattleUnit *unit,
	const std::string &event) const
{
	const VoiceEventRule *rule = resolveEvent(_mod, unit, event);
	return rule ? rule->lines.size() : 0;
}

int CalypsoVoiceManager::cooldownMs(const BattleUnit *unit,
	const std::string &event) const
{
	const VoiceEventRule *rule = resolveEvent(_mod, unit, event);
	return rule ? rule->cooldownMs : 0;
}

int CalypsoVoiceManager::priority(const BattleUnit *unit,
	const std::string &event) const
{
	const VoiceEventRule *rule = resolveEvent(_mod, unit, event);
	return rule ? rule->priority : 0;
}

int CalypsoVoiceManager::probability(const BattleUnit *unit,
	const std::string &event) const
{
	const VoiceEventRule *rule = resolveEvent(_mod, unit, event);
	return rule ? rule->probability : 0;
}

std::size_t CalypsoVoiceManager::selectLine(const BattleUnit *unit,
	const std::string &event, std::size_t eventIndex)
{
	const RuleVoiceProfile *profile = resolveProfile(_mod, unit);
	const VoiceEventRule *rule = resolveEvent(_mod, unit, event);
	if (!profile || !rule || rule->lines.empty())
	{
		return CalypsoVoiceLineBag::noLine();
	}

	const int unitId = unit->getId();
	const std::string bagName = profile->getId() + "\n" + event;
	CalypsoVoiceLineBag &bag = _lineBags[std::make_pair(unitId, bagName)];
	const std::uint32_t seedBase = 0x9e3779b9u
		^ (static_cast<std::uint32_t>(unitId) * 0x85ebca6bu)
		^ (static_cast<std::uint32_t>(eventIndex) * 0xc2b2ae35u);
	return bag.next(rule->weights, seedBase);
}

std::string CalypsoVoiceManager::clipPath(const BattleUnit *unit,
	const std::string &event, std::size_t lineIndex) const
{
	const RuleVoiceProfile *profile = resolveAudioProfile(
		_mod, unit, _availablePacks);
	return buildClipPath(profile, event, lineIndex, _underwater);
}

std::string CalypsoVoiceManager::fallbackClipPath(const BattleUnit *unit,
	const std::string &event, std::size_t lineIndex) const
{
	const RuleVoiceProfile *profile = resolveProfile(_mod, unit);
	if (!profile || profile->getFallbackProfile().empty()
		|| _availablePacks.find(profile->getPack()) == _availablePacks.end())
	{
		return std::string();
	}
	const RuleVoiceProfile *fallback = _mod->getVoiceProfile(
		profile->getFallbackProfile());
	if (!fallback
		|| _availablePacks.find(fallback->getPack()) == _availablePacks.end())
	{
		return std::string();
	}
	return buildClipPath(fallback, event, lineIndex, _underwater);
}

CalypsoVoiceRequestResult CalypsoVoiceManager::playEvent(
	const PendingEvent &request, std::uint32_t nowMs)
{
	CalypsoVoiceRequestResult result;
	result.unit = request.unit;
	result.event = request.event;
	const std::size_t line = selectLine(request.unit, request.event,
		eventSeedIndex(request.event));
	if (line == CalypsoVoiceLineBag::noLine())
	{
		result.status = CalypsoVoiceRequestStatus::Suppressed;
		result.reason = "missing_ruleset_event";
		return result;
	}

	const VoiceEventRule *rule = resolveEvent(_mod, request.unit, request.event);
	result.lineId = rule->lines[line];
	result.clipPath = clipPath(request.unit, request.event, line);
	_subtitleUnit = request.unit;
	_subtitleLineId = result.lineId;
	_subtitleTactical = !request.flavor;
	_subtitleStartedMs = nowMs;
	_subtitleDurationMs = 2500;

	CalypsoVoicePlayResult playResult = CalypsoVoicePlayResult::LoadFailed;
	std::uint32_t audioDurationMs = 0;
	const bool hasAudio = audioAvailable(request.unit);
	if (hasAudio)
	{
		playResult = playClip(result.clipPath, request.priority, &audioDurationMs);
		if (playResult == CalypsoVoicePlayResult::LoadFailed)
		{
			const std::string fallbackPath = fallbackClipPath(
				request.unit, request.event, line);
			if (!fallbackPath.empty() && fallbackPath != result.clipPath)
			{
				result.clipPath = fallbackPath;
				playResult = playClip(result.clipPath, request.priority,
					&audioDurationMs);
			}
		}
	}
	if (playResult == CalypsoVoicePlayResult::Played)
	{
		_subtitleDurationMs = std::max<std::uint32_t>(2500u,
			audioDurationMs + 350u);
		result.status = CalypsoVoiceRequestStatus::Played;
		result.reason = "played";
	}
	else
	{
		result.status = CalypsoVoiceRequestStatus::SubtitleOnly;
		result.reason = !hasAudio ? "pack_unavailable"
			: (playResult == CalypsoVoicePlayResult::LoadFailed
				? "load_failed" : "playback_failed");
	}

	const int unitId = request.unit->getId();
	_hasLastGlobal = true;
	_lastGlobal = nowMs;
	_lastSpeaker[unitId] = nowMs;
	_lastEvent[std::make_pair(unitId, request.event)] = nowMs;
	return result;
}

CalypsoVoiceRequestResult CalypsoVoiceManager::dispatch(
	const PendingEvent &request, std::uint32_t nowMs,
	bool requestOccupiesPendingSlot)
{
	CalypsoVoiceRequestResult result;
	result.unit = request.unit;
	result.event = request.event;
	if (!request.unit || !hasEvent(request.unit, request.event))
	{
		result.status = CalypsoVoiceRequestStatus::Suppressed;
		result.reason = "missing_ruleset_event";
		return result;
	}

	CalypsoVoiceEventSubmission submission;
	submission.nowMs = nowMs;
	submission.hasLastGlobalMs = _hasLastGlobal;
	submission.lastGlobalMs = _lastGlobal;
	const int unitId = request.unit->getId();
	const auto speaker = _lastSpeaker.find(unitId);
	submission.hasLastSpeakerMs = speaker != _lastSpeaker.end();
	submission.lastSpeakerMs = submission.hasLastSpeakerMs ? speaker->second : 0;
	const auto event = _lastEvent.find(std::make_pair(unitId, request.event));
	submission.hasLastEventMs = event != _lastEvent.end();
	submission.lastEventMs = submission.hasLastEventMs ? event->second : 0;
	submission.eventCooldownMs = static_cast<std::uint32_t>(cooldownMs(
		request.unit, request.event));
	submission.channelPlaying = isPlaying();
	submission.currentPriority = currentPriority();
	submission.pendingPresent = !requestOccupiesPendingSlot && hasPending();
	submission.pendingPriority = submission.pendingPresent ? _pending.priority : 0;
	submission.requestedPriority = request.priority;
	submission.isFlavor = request.flavor;
	submission.isSafety = request.safety;
	submission.isForce = request.force;

	switch (calypsoDecideVoiceEvent(submission))
	{
		case CalypsoVoiceDecision::PlayNow:
		{
			BattleUnit *displacedUnit = nullptr;
			std::string displacedEvent;
			if (request.force || request.safety)
			{
				if (_pending.unit == request.unit
					|| (request.safety && _pending.priority < request.priority))
				{
					displacedUnit = _pending.unit;
					displacedEvent = _pending.event;
					_pending = PendingEvent{};
				}
			}
			result = playEvent(request, nowMs);
			result.displacedUnit = displacedUnit;
			result.displacedEvent = displacedEvent;
			return result;
		}

		case CalypsoVoiceDecision::Queue:
			if (requestOccupiesPendingSlot)
			{
				result.status = CalypsoVoiceRequestStatus::Queued;
				result.reason = "pending";
				return result;
			}
			_pending = request;
			result.status = CalypsoVoiceRequestStatus::Queued;
			result.reason = "queued";
			return result;

		case CalypsoVoiceDecision::ReplaceQueued:
			result.displacedUnit = _pending.unit;
			result.displacedEvent = _pending.event;
			_pending = request;
			result.status = CalypsoVoiceRequestStatus::Queued;
			result.reason = "queue_replaced";
			return result;

		case CalypsoVoiceDecision::Suppress:
		default:
			result.status = CalypsoVoiceRequestStatus::Suppressed;
			result.reason = "arbitration";
			return result;
	}
}

CalypsoVoiceRequestResult CalypsoVoiceManager::submit(BattleUnit *unit,
	const std::string &event, std::uint32_t nowMs, bool flavor, bool safety,
	bool force)
{
	CalypsoVoiceRequestResult result;
	result.unit = unit;
	result.event = event;
	if (!unit || !hasEvent(unit, event))
	{
		result.status = CalypsoVoiceRequestStatus::Suppressed;
		result.reason = "missing_ruleset_event";
		return result;
	}

	const int eventProbability = probability(unit, event);
	if (!force && eventProbability < 100
		&& static_cast<int>(calypsoVoiceNextRandom(_cosmeticState) % 100u)
			>= eventProbability)
	{
		result.status = CalypsoVoiceRequestStatus::Suppressed;
		result.reason = "probability";
		return result;
	}

	PendingEvent request;
	request.unit = unit;
	request.event = event;
	request.priority = priority(unit, event);
	request.flavor = flavor;
	request.safety = safety;
	request.force = force;
	return dispatch(request, nowMs, false);
}

CalypsoVoiceRequestResult CalypsoVoiceManager::update(std::uint32_t nowMs,
	bool playbackAllowed)
{
	if (!hasPending())
	{
		return CalypsoVoiceRequestResult{};
	}
	if (!playbackAllowed)
	{
		CalypsoVoiceRequestResult result;
		result.status = CalypsoVoiceRequestStatus::Suppressed;
		result.unit = _pending.unit;
		result.event = _pending.event;
		result.reason = "muted";
		_pending = PendingEvent{};
		return result;
	}
	const PendingEvent request = _pending;
	CalypsoVoiceRequestResult result = dispatch(request, nowMs, true);
	if (result.status != CalypsoVoiceRequestStatus::Queued)
	{
		_pending = PendingEvent{};
	}
	return result;
}

CalypsoVoiceSubtitleState CalypsoVoiceManager::subtitle(
	std::uint32_t nowMs) const
{
	CalypsoVoiceSubtitleState result;
	result.active = _subtitleUnit && !_subtitleLineId.empty()
		&& calypsoVoiceElapsedMs(nowMs, _subtitleStartedMs) < _subtitleDurationMs;
	result.tactical = _subtitleTactical;
	result.unit = _subtitleUnit;
	result.lineId = _subtitleLineId;
	return result;
}

bool CalypsoVoiceManager::isPlaying() const
{
	return Mix_Playing(4) != 0;
}

void CalypsoVoiceManager::halt()
{
	Mix_HaltChannel(4);
	_currentPriority = 0;
}

bool CalypsoVoiceManager::makeCacheRoom(std::size_t requiredBytes)
{
	while (_decodedBytes + requiredBytes > DECODED_CACHE_LIMIT)
	{
		Mix_Chunk *playing = isPlaying() ? Mix_GetChunk(4) : nullptr;
		auto oldest = _chunks.end();
		for (auto it = _chunks.begin(); it != _chunks.end(); ++it)
		{
			if (it->second.chunk == playing)
			{
				continue;
			}
			if (oldest == _chunks.end()
				|| it->second.lastUsed < oldest->second.lastUsed)
			{
				oldest = it;
			}
		}
		if (oldest == _chunks.end())
		{
			return false;
		}
		_decodedBytes -= oldest->second.chunk->alen;
		Mix_FreeChunk(oldest->second.chunk);
		_chunks.erase(oldest);
	}
	return true;
}

Mix_Chunk *CalypsoVoiceManager::loadClip(const std::string &relativePath)
{
	const std::string path = std::string(VOICE_ROOT) + relativePath;
	auto loaded = _chunks.find(path);
	if (loaded != _chunks.end())
	{
		loaded->second.lastUsed = ++_cacheClock;
		return loaded->second.chunk;
	}
	if (_failedLoads.find(path) != _failedLoads.end())
	{
		return nullptr;
	}

	SDL_RWops *rw = em_file_to_rwops(path.c_str());
	if (!rw)
	{
		_failedLoads.insert(path);
		Log(LOG_ERROR) << LOG_TAG << " failed to open " << path;
		return nullptr;
	}
	Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, SDL_TRUE);
	if (!chunk)
	{
		_failedLoads.insert(path);
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
	_decodedBytes += chunk->alen;
	_chunks[path] = CachedChunk{chunk, ++_cacheClock};
	return chunk;
}

CalypsoVoicePlayResult CalypsoVoiceManager::playClip(
	const std::string &relativePath, int priority, std::uint32_t *durationMs)
{
	Mix_Chunk *chunk = loadClip(relativePath);
	if (!chunk)
	{
		return CalypsoVoicePlayResult::LoadFailed;
	}
	if (Mix_PlayChannel(4, chunk, 0) != 4)
	{
		return CalypsoVoicePlayResult::PlaybackFailed;
	}
	if (durationMs)
	{
		int frequency = 0;
		int channels = 0;
		Uint16 format = 0;
		const int bytesPerSample = Mix_QuerySpec(&frequency, &format, &channels)
			? SDL_AUDIO_BITSIZE(format) / 8 : 0;
		const std::uint64_t bytesPerSecond = static_cast<std::uint64_t>(frequency)
			* static_cast<std::uint64_t>(channels)
			* static_cast<std::uint64_t>(bytesPerSample);
		*durationMs = bytesPerSecond == 0 ? 0u
			: static_cast<std::uint32_t>(
				static_cast<std::uint64_t>(chunk->alen) * 1000u / bytesPerSecond);
	}
	_currentPriority = priority;
	return CalypsoVoicePlayResult::Played;
}

void CalypsoVoiceManager::releaseAudio()
{
	Mix_HaltChannel(4);
	for (auto &entry : _chunks)
	{
		Mix_FreeChunk(entry.second.chunk);
	}
	_chunks.clear();
	_failedLoads.clear();
	_decodedBytes = 0;
	_cacheClock = 0;
	_currentPriority = 0;
}

}

#endif
