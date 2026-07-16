#pragma once

#if defined(__EMSCRIPTEN__) && defined(CALYPSO_VOICE_G0_5)

namespace OpenXcom
{

class BattleUnit;

/**
 * Disposable Phase-44 G0.5 voice-bark gameplay spike.
 *
 * This is intentionally not the production voice system. It exists only in
 * CALYPSO_VOICE_G0_5 builds and owns the fixed ten-clip English pilot slice.
 */
class CalypsoVoiceG05
{
public:
	static void beginMission();
	static void endMission();

	/// Returns true when the spike owns selection audio for this unit.
	static bool handleSelection(BattleUnit *unit, bool sameUnit);
	static void onAlienSpotted(BattleUnit *spotter, BattleUnit *hostile);
	static void onDamage(BattleUnit *attacker, BattleUnit *target, int healthDamage, int stunDamage);
	/// Resolve a deferred wounded bark after the engine classifies the casualty.
	static void onCasualtyResolved(BattleUnit *unit);
	/// Returns true when the pilot death clip replaced the stock death sound.
	static bool onDeath(BattleUnit *unit);
};

}

#endif
