#pragma once

#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_P_EN)

#include "CalypsoVoiceLineSelection.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>

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

/** Production rules, line selection, and lazy playback for semantic voice events. */
class CalypsoVoiceManager
{
private:
	struct CachedChunk
	{
		Mix_Chunk *chunk = nullptr;
		std::uint64_t lastUsed = 0;
	};

	const Mod *_mod = nullptr;
	std::map<std::pair<int, std::string>, CalypsoVoiceLineBag> _lineBags;
	std::map<std::string, CachedChunk> _chunks;
	std::set<std::string> _failedLoads;
	std::size_t _decodedBytes = 0;
	std::uint64_t _cacheClock = 0;
	int _currentPriority = 0;

	bool makeCacheRoom(std::size_t requiredBytes);
	Mix_Chunk *loadClip(const std::string &relativePath);

public:
	void beginMission(const Mod *mod);
	void endMission();

	bool hasEvent(const BattleUnit *unit, const std::string &event) const;
	std::size_t lineCount(const BattleUnit *unit, const std::string &event) const;
	int cooldownMs(const BattleUnit *unit, const std::string &event) const;
	int priority(const BattleUnit *unit, const std::string &event) const;
	int probability(const BattleUnit *unit, const std::string &event) const;
	std::size_t selectLine(const BattleUnit *unit, const std::string &event,
		std::size_t eventIndex);
	std::string dryClipPath(const BattleUnit *unit, const std::string &event,
		std::size_t lineIndex) const;

	bool isPlaying() const;
	int currentPriority() const { return _currentPriority; }
	void halt();
	CalypsoVoicePlayResult playClip(const std::string &relativePath, int priority);
	void releaseAudio();
};

}

#endif
