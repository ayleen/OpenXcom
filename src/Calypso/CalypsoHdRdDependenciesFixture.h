#pragma once

/* Fixed dependency-tree fixtures for RR3-002 D01-D06. */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdRdFixture.h"

#include <memory>

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleManufacture.h"
#include "../Savegame/Base.h"

namespace OpenXcom
{
namespace Calypso
{

inline bool calypsoHdRdDependenciesProfileValid(int profileId)
{
	return profileId >= 220 && profileId <= 225;
}

inline RdAcceptanceFixture makeRdDependenciesFixture(Game *game, int profileId)
{
	RdAcceptanceFixture fixture;
	if (!game || !game->getMod() || !calypsoHdRdDependenciesProfileValid(profileId))
		return fixture;
	const char *ruleName = nullptr;
	switch (profileId)
	{
	case 220: ruleName = "STR_CALYPSO_RD_DEP_ROOT_RULE"; break;
	case 221: ruleName = "STR_CALYPSO_RD_DEP_CYCLE_RULE"; break;
	case 222:
	case 223: ruleName = "STR_CALYPSO_RD_DEP_SECRET_RULE"; break;
	case 224: ruleName = "STR_CALYPSO_RD_DEP_ROOT_RULE"; break;
	case 225: ruleName = "STR_CALYPSO_RD_DEP_NONE_RULE"; break;
	default: return fixture;
	}
	fixture.manufacture = game->getMod()->getManufacture(ruleName, false);
	if (!fixture.manufacture) return RdAcceptanceFixture();
	fixture.base = new Base(game->getMod());
	fixture.base->setName("Batumi");
	if (profileId == 224)
	{
		const bool previous = Options::oxceDisableProductionDependencyTree;
		Options::oxceDisableProductionDependencyTree = true;
		fixture.restoreOptions = [previous]() {
			Options::oxceDisableProductionDependencyTree = previous;
		};
	}
	return fixture;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
