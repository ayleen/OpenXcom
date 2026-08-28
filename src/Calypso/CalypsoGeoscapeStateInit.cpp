#ifdef __EMSCRIPTEN__
#include "CalypsoGeoscapeStateInit.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/Globe.h"
#include "CalypsoGeoscapeHd.h"
#include "CalypsoGeoscapeHdRuntime.h"
#include "CalypsoGeoscapeHdShell.h"
#include "CalypsoHdScreenRenderer.h"
#include "CalypsoHdUiOverlay.h"
#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "../Mod/Mod.h"

extern "C" int g_calypsoGeoscapeHdPreview;
extern "C" int g_calypsoGlobeGpuDirect;

namespace OpenXcom { namespace Calypso {
void calypsoGeoscapeStateInitHd(GeoscapeState &state)
{
	state.calypsoChecklistBuild();
	CalypsoGeoscapeHd::applyTtf(&state);
	CalypsoGeoscapeHd::layout(&state);
	CalypsoGeoscapeHdShell::apply(&state);
	const bool canonicalListed = state._game->getMod()->isHdUiFamilyEnabled("F16");
	const bool previewListed = Calypso::calypsoGeoscapeHdPreviewFamilyEnabled(canonicalListed, ::g_calypsoGeoscapeHdPreview != 0);
	if (previewListed) {
		state._calypsoHdRenderer = new Calypso::CalypsoHdScreenRenderer(&state, Calypso::CalypsoHdScreenRenderModel{}, Calypso::CalypsoHdScreenRenderMode::GeoscapeLiveChrome);
		Calypso::CalypsoHdUiOverlay::instance().registerAdapter(state._calypsoHdRenderer);
	}
	if (previewListed || ::g_calypsoGlobeGpuDirect != 0) state._globe->setGpuDirect(true);
	{
		using OpenXcom::Calypso::calypsoGeoscapeHdGateDecision;
		const bool listed = Calypso::calypsoGeoscapeHdPreviewFamilyEnabled(state._game->getMod()->isHdUiFamilyEnabled("F16"), ::g_calypsoGeoscapeHdPreview != 0);
		const auto decision = calypsoGeoscapeHdGateDecision(true, listed, true, true);
		Log(LOG_INFO) << "[HD] geoscape command shell gate: " << decision.reason;
	}
}
} }
#endif
