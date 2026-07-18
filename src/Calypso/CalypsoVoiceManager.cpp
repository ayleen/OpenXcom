#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_P_EN)

#include "CalypsoVoiceManager.h"

#include "RuleVoiceProfile.h"
#include "../Engine/FileMap.h"
#include "../Engine/Logger.h"
#include "../Mod/Mod.h"
#include "../Savegame/BattleUnit.h"

#include <SDL.h>
#include <SDL_mixer.h>

namespace OpenXcom
{
namespace
{

constexpr const char *VOICE_ROOT = "/game/voice-packs/";
constexpr const char *LOG_TAG = "[VOICE_P_EN]";
constexpr std::size_t DECODED_CACHE_LIMIT = 4u * 1024u * 1024u;

const RuleVoiceProfile *resolveProfile(const Mod *mod, const BattleUnit *unit)
{
	return mod && unit ? mod->getVoiceProfile(unit->getVoiceProfile()) : nullptr;
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

}

void CalypsoVoiceManager::beginMission(const Mod *mod)
{
	releaseAudio();
	_mod = mod;
	_lineBags.clear();
}

void CalypsoVoiceManager::endMission()
{
	releaseAudio();
	_mod = nullptr;
	_lineBags.clear();
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

std::string CalypsoVoiceManager::dryClipPath(const BattleUnit *unit,
	const std::string &event, std::size_t lineIndex) const
{
	const RuleVoiceProfile *profile = resolveProfile(_mod, unit);
	const VoiceEventRule *rule = resolveEvent(_mod, unit, event);
	if (!profile || !rule || lineIndex >= rule->lines.size())
	{
		return std::string();
	}

	std::string path = profile->getDryPathTemplate();
	replaceToken(path, "{profile}", profile->getId());
	replaceToken(path, "{line}", rule->lines[lineIndex]);
	return profile->getPack() + "/" + path;
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
	const std::string &relativePath, int priority)
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
