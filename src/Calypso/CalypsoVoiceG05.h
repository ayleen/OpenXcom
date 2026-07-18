#pragma once

#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_G0_5)

namespace OpenXcom
{

class BattleUnit;

/**
 * Disposable Phase-44 G0.5 voice-bark gameplay spike.
 *
 * This is intentionally not the production voice system. It exists only in
 * CALYPSO_VOICE_G0_5 builds and owns the approved English gameplay corpus.
 */
class CalypsoVoiceG05
{
public:
	static void beginMission();
	static void endMission();
	/// Accept an asynchronous browser pack result only for the active mission.
	static bool onPackResult(unsigned int missionEpoch, bool available);

	/// Returns true when the spike owns selection audio for this unit.
	static bool handleSelection(BattleUnit *unit, bool sameUnit);
	/// Returns true when the spike owns movement acknowledgement for this unit.
	static bool handleMoveOrder(BattleUnit *unit);
	static void onWeaponReady(BattleUnit *unit);
	static void onOutOfAmmo(BattleUnit *unit);
	static void onAlienSpotted(BattleUnit *spotter, BattleUnit *hostile);
	static void onGrenadeThrown(BattleUnit *unit);
	static void onAttackStarted(BattleUnit *unit);
	static void onDamage(BattleUnit *attacker, BattleUnit *target, int healthDamage, int stunDamage);
	static void onAttackFinished(BattleUnit *unit);
	/// Returns true when the pilot replaces the stock panic/berserk voice.
	static bool onPanic(BattleUnit *unit);
	/// Resolve a deferred wounded bark after the engine classifies the casualty.
	static void onCasualtyResolved(BattleUnit *unit);
	/// Returns true when the pilot death clip replaced the stock death sound.
	static bool onDeath(BattleUnit *unit);
};

}

#endif
