#ifdef __EMSCRIPTEN__
/*
 * Phase 41 (Calypso) -- concrete-scene registration.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 */

#include "CalypsoSceneRegistry.h"
#include "CalypsoDirector.h"
#include "CalypsoPrologueScene.h"

namespace OpenXcom
{

void registerCalypsoScenes()
{
	CalypsoDirector::get().registerScene("STR_CALYPSO_PROLOGUE", []() -> CalypsoScene * {
		return new CalypsoPrologueScene();
	});
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
