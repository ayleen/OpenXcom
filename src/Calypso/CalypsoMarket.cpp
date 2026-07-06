/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Phase 38 (Calypso) -- implementation of the CalypsoMarket delegation layer.
 *
 * See CalypsoMarket.h for design notes. All entry points short-circuit to a
 * safe default when the economy is not active, so callers in Basescape states
 * can unconditionally #ifdef __EMSCRIPTEN__ a one-liner call into this module.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoMarket.h"
#include "CalypsoEconomy.h"

#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"

namespace
{
	// Returns the active economy or nullptr; also outputs the rules + savedgame.
	OpenXcom::Calypso::Economy* eco(OpenXcom::Game* game)
	{
		if (!game) return nullptr;
		auto* e = game->getSavedGame()->getCalypsoEconomy();
		return (e && e->active()) ? e : nullptr;
	}
}

namespace OpenXcom
{
namespace Calypso
{

bool marketActive(Game* game)
{
	return eco(game) != nullptr;
}

bool marketSellsToPlayer(Game* game, const std::string& cp, const RuleItem* item)
{
	Economy* e = eco(game);
	return e && e->sellsToPlayer(cp, item, game->getMod()->getCalypsoEconomyRules());
}

bool marketBuysFromPlayer(Game* game, const std::string& cp, const RuleItem* item)
{
	Economy* e = eco(game);
	return e && e->buysFromPlayer(cp, item, game->getMod()->getCalypsoEconomyRules());
}

bool marketCanBuy(Game* game, const std::string& cp, const RuleItem* item)
{
	Economy* e = eco(game);
	if (!e) return false;
	const EconomyRules& r = game->getMod()->getCalypsoEconomyRules();
	return e->sellsToPlayer(cp, item, r) && e->getStock(cp, item, game->getSavedGame(), r) > 0;
}

bool marketCanSell(Game* game, const std::string& cp, const RuleItem* item)
{
	Economy* e = eco(game);
	if (!e) return false;
	const EconomyRules& r = game->getMod()->getCalypsoEconomyRules();
	return e->buysFromPlayer(cp, item, r) && e->getDemand(cp, item, game->getSavedGame(), r) > 0;
}

int64_t marketBuyPrice(Game* game, const std::string& cp, const RuleItem* item)
{
	Economy* e = eco(game);
	return e ? e->buyPrice(cp, item, game->getMod()->getCalypsoEconomyRules()) : 0;
}

int64_t marketSellPrice(Game* game, const std::string& cp, const RuleItem* item)
{
	Economy* e = eco(game);
	return e ? e->sellPrice(cp, item, game->getMod()->getCalypsoEconomyRules()) : 0;
}

int marketStock(Game* game, const std::string& cp, const RuleItem* item)
{
	Economy* e = eco(game);
	return e ? e->getStock(cp, item, game->getSavedGame(), game->getMod()->getCalypsoEconomyRules()) : 0;
}

int marketDemand(Game* game, const std::string& cp, const RuleItem* item)
{
	Economy* e = eco(game);
	return e ? e->getDemand(cp, item, game->getSavedGame(), game->getMod()->getCalypsoEconomyRules()) : 0;
}

void marketRecordBuy(Game* game, const std::string& cp, const RuleItem* item, int qty)
{
	Economy* e = eco(game);
	if (e) e->recordPurchase(cp, item, qty);
}

void marketRecordSell(Game* game, const std::string& cp, const RuleItem* item, int qty)
{
	Economy* e = eco(game);
	if (e) e->recordSale(cp, item, qty, game->getMod()->getCalypsoEconomyRules());
}

} // namespace Calypso
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
