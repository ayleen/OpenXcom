#ifdef __EMSCRIPTEN__
#include "CalypsoF20ConfirmCydoniaUi.h"
#include "CalypsoHdUiOverlay.h"
#include "../Engine/Game.h"
#include "../Geoscape/ConfirmCydoniaState.h"
#include "../Interface/Window.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/Mod.h"
#include "../Savegame/Craft.h"

namespace OpenXcom { namespace Calypso {
void CalypsoF20ConfirmCydoniaUi::configure(ConfirmCydoniaState &s, bool allow)
{
	if (!allow || !s._craft || !s._craft->getBase() || !s._game || !s._game->getMod())
		CalypsoHdUiOverlay::instance().failHdRoute("Final mission review is unavailable");
	auto *mod = s._game->getMod();
	const AlienDeployment *final = nullptr;
	for (const auto &id : mod->getDeploymentsList())
	{
		auto *deployment = mod->getDeployment(id);
		if (deployment && deployment->isFinalDestination()) { final = deployment; break; }
	}
	if (!final) CalypsoHdUiOverlay::instance().failHdRoute(s.tr("STR_CAL_DROP_NO_FINAL"));
	const std::string name = s.tr(final->getType());
	Facts facts = {{s.tr("STR_CAL_DROP_DEPLOYMENT"), name}, {s.tr("STR_CAL_DROP_RACE"), s.tr(final->getRace())},
		{s.tr("STR_CAL_DROP_UNITS"), std::to_string(s._craft->getNumTotalUnits())}};
	s._screen = true;
	s._window->finishPopupForHd();
	s._hdAdapter = new CalypsoF20ConfirmCydoniaUi(s, s._craft, name, std::move(facts), s._btnYes, s._btnNo,
		nullptr, (ActionHandler)&ConfirmCydoniaState::reviewTabClick, "confirmCydonia");
}

bool CalypsoF20ConfirmCydoniaUi::resize(ConfirmCydoniaState &s)
{
	if (!s._hdAdapter) return false;
	s._hdAdapter->refresh();
	return true;
}
} }
#endif
