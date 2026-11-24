#ifdef __EMSCRIPTEN__
#include "CalypsoF20ConfirmLandingUi.h"
#include "CalypsoHdUiOverlay.h"
#include "../Engine/Game.h"
#include "../Geoscape/ConfirmLandingState.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleStartingCondition.h"
#include "../Mod/Texture.h"
#include "../Savegame/AlienBase.h"
#include "../Savegame/Craft.h"
#include "../Savegame/MissionSite.h"
#include "../Savegame/Ufo.h"

namespace OpenXcom { namespace Calypso {
void CalypsoF20ConfirmLandingUi::configure(ConfirmLandingState &s, bool allow)
{
	if (!allow || !s._craft || !s._craft->getBase() || !s._craft->getDestination()
		|| !s._game || !s._game->getMod())
		CalypsoHdUiOverlay::instance().failHdRoute("Landing review is unavailable");
	auto *target = s._craft->getDestination();
	Facts facts;
	if (auto *ufo = dynamic_cast<Ufo*>(target))
		facts.emplace_back(s.tr("STR_CAL_DROP_DEPLOYMENT"), s.tr(ufo->getStatus() == Ufo::CRASHED
			? "STR_UFO_CRASH_RECOVERY" : "STR_UFO_GROUND_ASSAULT"));
	else if (auto *site = dynamic_cast<MissionSite*>(target))
		facts.emplace_back(s.tr("STR_CAL_DROP_DEPLOYMENT"), s.tr(site->getDeployment()->getType()));
	else if (auto *base = dynamic_cast<AlienBase*>(target))
		facts.emplace_back(s.tr("STR_CAL_DROP_DEPLOYMENT"), s.tr(base->getDeployment()->getType()));
	const char *light = s._shade <= 0 ? "STR_CAL_DROP_DAY" :
		s._shade > s._game->getMod()->getMaxDarknessToSeeUnits() ? "STR_CAL_DROP_NIGHT" : "STR_CAL_DROP_DUSK";
	facts.emplace_back(s.tr("STR_CAL_DROP_LIGHT"), s.tr(light));
	facts.emplace_back(s.tr("STR_CAL_DROP_SHADE"), std::to_string(s._shade));
	if (s._missionTexture && s._missionTexture->isFakeUnderwater())
		facts.emplace_back(s.tr("STR_CAL_DROP_UNDERWATER"), s.tr("STR_YES"));
	const auto *condition = s.landingStartingCondition();
	if (condition && condition->getDestroyRequiredItems())
	{
		for (const auto &item : condition->getRequiredItems())
		{
			facts.emplace_back(s.tr(item.first), s.tr("STR_CAL_DROP_CONSUMES").arg(item.second));
		}
	}
	auto *patrol = new TextButton(1, 1);
	s.add(patrol, "button", "confirmLanding");
	patrol->setText(s.tr("STR_PATROL"));
	patrol->onMouseClick((ActionHandler)&ConfirmLandingState::reviewPatrolClick);
	s._screen = true;
	s._window->finishPopupForHd();
	s._hdAdapter = new CalypsoF20ConfirmLandingUi(s, s._craft, target->getName(s._game->getLanguage()),
		std::move(facts), s._btnYes, s._btnNo, patrol, (ActionHandler)&ConfirmLandingState::reviewTabClick, "confirmLanding");
}
bool CalypsoF20ConfirmLandingUi::resize(ConfirmLandingState &s)
{
	if (!s._hdAdapter) return false;
	s._hdAdapter->refresh();
	return true;
}
} }
#endif
