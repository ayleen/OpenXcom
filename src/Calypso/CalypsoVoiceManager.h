#pragma once

#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_P_EN)

#include "CalypsoVoiceArbitration.h"
#include "CalypsoVoiceLineSelection.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct Mix_Chunk;

namespace OpenXcom
{

class BattleUnit;
class Mod;

enum class CalypsoVoicePlayResult
{
	Played,
	LoadFailed,
	PlaybackFailed
};

enum class CalypsoVoiceRequestStatus
{
	Idle,
	Played,
	SubtitleOnly,
	Queued,
	Suppressed,
	LoadFailed,
	PlaybackFailed
};

struct CalypsoVoiceRequestResult
{
	CalypsoVoiceRequestStatus status = CalypsoVoiceRequestStatus::Idle;
	BattleUnit *unit = nullptr;
	std::string event;
	std::string lineId;
	std::string clipPath;
	std::string reason;
	BattleUnit *displacedUnit = nullptr;
	std::string displacedEvent;
};

struct CalypsoVoiceSubtitleState
{
	bool active = false;
	bool tactical = false;
	BattleUnit *unit = nullptr;
	std::string lineId;
};

/** Production rules, line selection, and lazy playback for semantic voice events. */
class CalypsoVoiceManager
{
private:
	struct CachedChunk
	{
		Mix_Chunk *chunk = nullptr;
		std::uint64_t lastUsed = 0;
	};
	struct PendingEvent
	{
		BattleUnit *unit = nullptr;
		std::string event;
		int priority = 0;
		bool flavor = false;
		bool safety = false;
		bool force = false;
	};

	const Mod *_mod = nullptr;
	bool _underwater = false;
	std::map<std::pair<int, std::string>, CalypsoVoiceLineBag> _lineBags;
	std::map<std::string, CachedChunk> _chunks;
	std::set<std::string> _failedLoads;
	std::set<std::string> _availablePacks;
	std::size_t _decodedBytes = 0;
	std::uint64_t _cacheClock = 0;
	int _currentPriority = 0;
	std::uint32_t _cosmeticState = 0x6d2b79f5u;
	bool _hasLastGlobal = false;
	std::uint32_t _lastGlobal = 0;
	std::map<int, std::uint32_t> _lastSpeaker;
	std::map<std::pair<int, std::string>, std::uint32_t> _lastEvent;
	PendingEvent _pending;
	BattleUnit *_subtitleUnit = nullptr;
	std::string _subtitleLineId;
	bool _subtitleTactical = false;
	std::uint32_t _subtitleStartedMs = 0;
	std::uint32_t _subtitleDurationMs = 0;

	bool makeCacheRoom(std::size_t requiredBytes);
	Mix_Chunk *loadClip(const std::string &relativePath);
	CalypsoVoiceRequestResult dispatch(const PendingEvent &request,
		std::uint32_t nowMs, bool requestOccupiesPendingSlot);
	CalypsoVoiceRequestResult playEvent(const PendingEvent &request,
		std::uint32_t nowMs);
	std::string fallbackClipPath(const BattleUnit *unit,
		const std::string &event, std::size_t lineIndex) const;

public:
	void beginMission(const Mod *mod, unsigned int missionEpoch, bool underwater);
	void endMission();
	std::set<std::string> requiredPacks(
		const std::vector<BattleUnit *> &units) const;
	void setPackAvailable(const std::string &pack, bool available);
	bool audioAvailable(const BattleUnit *unit) const;

	bool hasEvent(const BattleUnit *unit, const std::string &event) const;
	std::size_t lineCount(const BattleUnit *unit, const std::string &event) const;
	int cooldownMs(const BattleUnit *unit, const std::string &event) const;
	int priority(const BattleUnit *unit, const std::string &event) const;
	int probability(const BattleUnit *unit, const std::string &event) const;
	std::size_t selectLine(const BattleUnit *unit, const std::string &event,
		std::size_t eventIndex);
	std::string clipPath(const BattleUnit *unit, const std::string &event,
		std::size_t lineIndex) const;
	CalypsoVoiceRequestResult submit(BattleUnit *unit, const std::string &event,
		std::uint32_t nowMs, bool flavor, bool safety, bool force);
	CalypsoVoiceRequestResult update(std::uint32_t nowMs, bool playbackAllowed);
	CalypsoVoiceSubtitleState subtitle(std::uint32_t nowMs) const;
	bool hasPending() const { return _pending.unit != nullptr; }

	bool isPlaying() const;
	int currentPriority() const { return _currentPriority; }
	void halt();
	CalypsoVoicePlayResult playClip(const std::string &relativePath, int priority,
		std::uint32_t *durationMs = nullptr);
	void releaseAudio();
};

}

#endif
