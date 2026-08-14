#ifdef __EMSCRIPTEN__

#include "../Geoscape/GeoscapeState.h"

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Timer.h"
#include "../Mod/Mod.h"
#include "../Savegame/MissionSite.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Ufo.h"

#include <emscripten.h>

namespace OpenXcom
{

/**
 * Selects the browser-owned Geoscape music state. Priority is:
 * active interception > detected terror site > detected flying submarine > waiting.
 * The JS side performs the actual overlapping crossfade and keeps compressed
 * streams outside the WASM heap.
 */
void GeoscapeState::updateCalypsoMusicState()
{
	const bool controllerAvailable = EM_ASM_INT({
		const available = globalThis.calypsoGeoscapeMusicIsAvailable;
		return typeof available === 'function' ? available() : 0;
	}) != 0;
	if (!controllerAvailable)
	{
		// The normal GMGEO/GMINTER path remains active in browsers without
		// OGG/Vorbis or when the browser controller failed to install.
		return;
	}

	const int volume = Options::mute ? 0 : Options::musicVolume;
	if (volume != _calypsoMusicVolume)
	{
		const double normalizedVolume = Options::mute ? 0.0 : Game::volumeExponent(volume);
		const int accepted = EM_ASM_INT({
			const fn = globalThis.calypsoGeoscapeMusicSetVolume;
			return fn ? fn($0) : 0;
		}, normalizedVolume);
		if (accepted)
		{
			_calypsoMusicVolume = volume;
		}
	}

	int state = 1; // Waiting.
	if (!_dogfights.empty() || !_dogfightsToBeStarted.empty() || _dogfightStartTimer->isRunning())
	{
		state = 3; // Intercept.
	}
	else
	{
		bool terror = false;
		for (const auto* site : *_game->getSavedGame()->getMissionSites())
		{
			if (site->getDetected() && site->getMarkerName() == "STR_TERROR_SITE")
			{
				terror = true;
				break;
			}
		}

		if (terror)
		{
			state = 4; // Terror.
		}
		else
		{
			for (const auto* ufo : *_game->getSavedGame()->getUfos())
			{
				if (ufo->getDetected() && ufo->getStatus() == Ufo::FLYING)
				{
					state = 2; // Sonar.
					break;
				}
			}
		}
	}

	if (state != _calypsoMusicState)
	{
		// Also fades a track selected through OXCE's manual music picker. The
		// Mod intercept suppresses the random legacy replacement while the
		// browser controller is available.
		_game->getMod()->playMusic(state == 3 ? "GMINTER" : "GMGEO");
		const int accepted = EM_ASM_INT({
			const fn = globalThis.calypsoGeoscapeMusicSetState;
			return fn ? fn($0) : 0;
		}, state);
		if (accepted)
		{
			_calypsoMusicState = state;
		}
	}
}

} // namespace OpenXcom

#endif
