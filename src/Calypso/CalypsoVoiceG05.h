#pragma once

#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_G0_5)

#include <string>

namespace OpenXcom
{

class BattleUnit;
class SavedBattleGame;
struct BattleAction;
struct BattleActionAttack;

struct CalypsoVoiceSubtitleSnapshot
{
	bool active = false;
	bool tactical = false;
	BattleUnit *unit = nullptr;
	std::string lineId;
};

/**
 * Disposable Phase-44 G0.5 voice-bark gameplay spike.
 *
 * This is intentionally not the production voice system. It exists only in
 * CALYPSO_VOICE_G0_5 builds and owns the approved English gameplay corpus.
 */
class CalypsoVoiceG05
{
public:
	static void beginMission(SavedBattleGame *save);
	static void endMission();
	/// Drain the production manager's single pending slot from the battle tick.
	static void think();
	/// Current semantic subtitle selected by the production manager.
	static CalypsoVoiceSubtitleSnapshot subtitle(unsigned int nowMs);
	/// Accept an asynchronous browser pack result only for the active mission.
	static bool onPackResult(const std::string &pack,
		unsigned int missionEpoch, bool available);

	/// Returns true when the spike owns selection audio for this unit.
	static bool handleSelection(BattleUnit *unit, bool sameUnit);
	/// Returns true when the spike owns movement acknowledgement for this unit.
	static bool handleMoveOrder(BattleUnit *unit);
	static void onWeaponReady(BattleUnit *unit);
	static void onOutOfAmmo(BattleUnit *unit);
	static void onAlienSpotted(BattleUnit *spotter, BattleUnit *hostile);
	static void onGrenadeThrown(BattleUnit *unit);
	static void onAttackStarted(BattleAction &action);
	static void onDamage(const BattleActionAttack &attack, BattleUnit *target,
		int healthDamage, int stunDamage);
	static void onKill(const BattleActionAttack &attack, BattleUnit *victim,
		BattleUnit *creditedKiller);
	static void onAttackFinished(const BattleAction &action);
	/// Returns true when the pilot replaces the stock panic/berserk voice.
	static bool onPanic(BattleUnit *unit);
	/// Resolve a deferred wounded bark after the engine classifies the casualty.
	static void onCasualtyResolved(BattleUnit *unit);
	/// Returns true when the pilot death clip replaced the stock death sound.
	static bool onDeath(BattleUnit *unit);
	/// Fire the civilian flee bark (Phase 44 E3) on first escape entry; no-op
	/// for non-civilians, guards, or profiles that don't declare "flee".
	static void onCivilianFlee(BattleUnit *unit);
};

}

#endif
