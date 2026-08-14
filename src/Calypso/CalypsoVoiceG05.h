#pragma once

#if defined(__EMSCRIPTEN__) && (defined(CALYPSO_VOICE_G0_5) || defined(CALYPSO_VOICE_P_EN))

#include <string>

namespace OpenXcom
{

class BattleUnit;
class BattlescapeState;
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
 * Phase-44 voice-bark gameplay bridge.
 *
 * CALYPSO_VOICE_G0_5 retains the disposable pilot path; CALYPSO_VOICE_P_EN
 * routes the same gameplay events into the production ruleset/pack manager.
 */
class CalypsoVoiceG05
{
public:
	/// Returns a transient owner lease; same-save state reconstruction preserves
	/// one runtime until the final lease is released.
	static unsigned int beginMission(SavedBattleGame *save);
	/// Reset stage-local arbitration when a multi-stage deployment reuses the
	/// same SavedBattleGame instance for a newly generated map.
	static void onStageTransition(SavedBattleGame *save);
	static void endMission(unsigned int ownerToken);
	/// Drain the production manager's single pending slot from the battle tick.
	static void think();
	/// Current semantic subtitle selected by the production manager.
	static CalypsoVoiceSubtitleSnapshot subtitle(unsigned int nowMs);
#if defined(CALYPSO_VOICE_P_EN)
	static void applyVolume(int setting, bool enabled);
	static void createSubtitle(BattlescapeState *state, int screenWidth, int hudY);
	static void addSubtitle(BattlescapeState *state);
	static void styleSubtitle(BattlescapeState *state);
	static void updateSubtitle(BattlescapeState *state);
	static void resizeSubtitle(BattlescapeState *state);
#endif
	/// Accept an asynchronous browser pack result only for the active mission.
	static bool onPackResult(const std::string &pack,
		unsigned int missionEpoch, bool available);

	/// Returns true when the spike owns selection audio for this unit.
	static bool handleSelection(BattleUnit *unit, bool sameUnit);
	/// Returns true when the spike owns movement acknowledgement for this unit.
	static bool handleMoveOrder(BattleUnit *unit);
	/// Returns true when the custom bark owns the one-shot weapon response.
	static bool onWeaponReady(BattleUnit *unit, bool stockResponseExists);
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
	static bool onPanic(BattleUnit *unit, bool stockResponseExists);
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
