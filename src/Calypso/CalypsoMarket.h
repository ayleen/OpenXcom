#ifdef __EMSCRIPTEN__
#pragma once
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
 * Phase 38 (Calypso) -- thin delegation layer over Economy + EconomyRules.
 *
 * PurchaseState / SellState (task B4) call into these free functions so their
 * own bodies only gain tiny hooks (Phase 36 policy P5). Each entry point reads
 * the active Calypso::Economy off the SavedGame plus the EconomyRules off the
 * Mod and forwards the call, returning a safe default (false / 0) when the
 * market is inactive. This keeps the Basescape states free of any direct
 * Calypso dependencies beyond a single include + #ifdef'd call site.
 */

#include <string>
#include <cstdint>

namespace OpenXcom
{
class Game;
class RuleItem;

namespace Calypso
{
/// True when the Calypso counterparty market is active (economy mod loaded + on).
bool marketActive(Game* game);
/// Include this item in counterparty `cp`'s PURCHASE list? (they sell it to us AND stock remains)
bool marketCanBuy(Game* game, const std::string& cp, const RuleItem* item);
/// Include this item in counterparty `cp`'s SELL list? (they buy it from us AND demand remains)
bool marketCanSell(Game* game, const std::string& cp, const RuleItem* item);
/// Whether cp sells the item at all (ignores stock) -- for the gate-explanation UI line.
bool marketSellsToPlayer(Game* game, const std::string& cp, const RuleItem* item);
/// Whether cp buys the item at all (ignores demand).
bool marketBuysFromPlayer(Game* game, const std::string& cp, const RuleItem* item);
int64_t marketBuyPrice(Game* game, const std::string& cp, const RuleItem* item);
int64_t marketSellPrice(Game* game, const std::string& cp, const RuleItem* item);
int marketStock(Game* game, const std::string& cp, const RuleItem* item);    // remaining buy cap this month
int marketDemand(Game* game, const std::string& cp, const RuleItem* item);   // remaining sell cap this month
void marketRecordBuy(Game* game, const std::string& cp, const RuleItem* item, int qty);
void marketRecordSell(Game* game, const std::string& cp, const RuleItem* item, int qty);
} // namespace Calypso
} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
