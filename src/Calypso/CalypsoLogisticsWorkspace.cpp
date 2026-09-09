
#ifdef __EMSCRIPTEN__
#include "CalypsoLogisticsWorkspace.h"
#include "CalypsoMarketState.h"
#include "../Basescape/BasescapeState.h"
#include "../Basescape/PurchaseState.h"
#include "../Basescape/SellState.h"
#include "../Basescape/TransferBaseState.h"
#include "../Basescape/TransferItemsState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Transfer.h"
#include "../Savegame/Craft.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace OpenXcom { namespace Calypso {
namespace
{
CalypsoLogisticsWorkspaceSession g_session;
Game *g_game = nullptr;
Base *g_base = nullptr;
Base *g_destination = nullptr;

std::string identity(const TransferRow& row)
{
	return std::to_string(static_cast<int>(row.type)) + ":" + row.name;
}

template<typename Rows>
std::vector<CalypsoLogisticsDraftRow> draftRows(const Rows& rows)
{
	std::vector<CalypsoLogisticsDraftRow> result;
	result.reserve(rows.size());
	for (const auto& row : rows)
		result.push_back({identity(row), row.amount});
	return result;
}

template<typename Rows>
void restoreRows(Rows& rows, const CalypsoLogisticsDraft& draft)
{
	if (!draft.hasDraft) return;
	for (auto& row : rows)
	{
		const auto key = identity(row);
		for (const auto& saved : draft.rows)
			if (saved.identity == key)
			{
				row.amount = std::max(0, std::min(saved.amount, row.qtySrc));
				break;
			}
	}
}
}

class CalypsoLogisticsWorkspace
{
	static void snapshot(PurchaseState& state)
	{
		g_session.drafts().snapshot(CalypsoLogisticsTab::Purchase, draftRows(state._items));
	}
	static void snapshot(SellState& state)
	{
		g_session.drafts().snapshot(CalypsoLogisticsTab::Sell, draftRows(state._items));
	}
	static void snapshot(TransferItemsState& state)
	{
		g_session.drafts().snapshot(CalypsoLogisticsTab::Transfer, draftRows(state._items));
	}
	static void restore(PurchaseState& state)
	{
		restoreRows(state._items, g_session.drafts().get(CalypsoLogisticsTab::Purchase));
		state._total = 0; state._pQty = 0; state._cQty = 0; state._iQty = 0;
		for (const auto& row : state._items)
		{
			state._total += row.cost * row.amount;
			if (row.type == TRANSFER_SOLDIER || row.type == TRANSFER_SCIENTIST ||
				row.type == TRANSFER_ENGINEER) state._pQty += row.amount;
			else if (row.type == TRANSFER_CRAFT) state._cQty += row.amount;
			else if (row.type == TRANSFER_ITEM) state._iQty += row.size * row.amount;
		}
		state.updateList();
	}
	static void restore(SellState& state)
	{
		restoreRows(state._items, g_session.drafts().get(CalypsoLogisticsTab::Sell));
		state._total = 0; state._spaceChange = 0;
		for (const auto& row : state._items)
		{
			state._total += static_cast<int64_t>(row.cost) * row.amount;
			state._spaceChange += row.size * row.amount;
		}
		state.updateList();
	}
	static void restore(TransferItemsState& state)
	{
		restoreRows(state._items, g_session.drafts().get(CalypsoLogisticsTab::Transfer));
		state._total = 0; state._pQty = 0; state._cQty = 0; state._aQty = 0; state._iQty = 0;
		for (const auto& row : state._items)
		{
			state._total += row.cost * row.amount;
			if (row.type == TRANSFER_CRAFT) state._cQty += row.amount;
			else if (row.type == TRANSFER_ITEM) state._iQty += row.size * row.amount;
			else state._pQty += row.amount;
		}
		state.updateList();
	}
	static int tabIndex(TextButton* sender, TextButton* const (&tabs)[3])
	{
		for (int i = 0; i < 3; ++i) if (tabs[i] == sender) return i;
		return -1;
	}
	static void push(CalypsoLogisticsTab tab)
	{
		switch (tab)
		{
		case CalypsoLogisticsTab::Purchase: g_game->pushState(new PurchaseState(g_base)); break;
		case CalypsoLogisticsTab::Sell: g_game->pushState(new SellState(g_base, nullptr)); break;
		case CalypsoLogisticsTab::Transfer: g_game->pushState(new TransferBaseState(g_base, nullptr)); break;
		default: break;
		}
	}
	static void replace(CalypsoLogisticsTab tab)
	{
		if (!g_game || !g_base || !g_session.switchTo(tab)) return;
		g_game->popState();
		push(tab);
	}
public:
	static bool open(Game* game, Base* base, CalypsoLogisticsTab tab)
	{
		if (!game || !base || tab == CalypsoLogisticsTab::Unknown) return false;
		g_game = game; g_base = base; g_destination = nullptr;
		g_session.begin(tab);
		if (game->getSavedGame() && game->getSavedGame()->getCalypsoEconomy() &&
			game->getSavedGame()->getCalypsoEconomy()->active() &&
			(tab == CalypsoLogisticsTab::Purchase || tab == CalypsoLogisticsTab::Sell))
		{
			game->pushState(new CalypsoMarketState(base, tab == CalypsoLogisticsTab::Sell));
			return true;
		}
		push(tab);
		return true;
	}
	static void ensure(Game* game, Base* base, CalypsoLogisticsTab tab)
	{
		if (!game || !base || tab == CalypsoLogisticsTab::Unknown) return;
		if (!g_session.active() || g_game != game || g_base != base)
		{
			g_game = game; g_base = base; g_destination = nullptr; g_session.begin(tab);
		}
	}
	static void rememberDestination(Base* destination)
	{
		g_destination = destination;
		g_session.rememberDestination(destination ? destination->getName() : std::string());
	}
	static void restoreDestination(TransferBaseState& state)
	{
		if (!g_session.destination().empty())
			for (std::size_t i = 0; i < state._bases.size(); ++i)
				if (state._bases[i] && state._bases[i]->getName() == g_session.destination())
				{
					state._lstBases->setSelectedRow(static_cast<int>(i));
					break;
				}
	}
	static void restore(PurchaseState& state, int) { restore(state); }
	static void restore(SellState& state, int) { restore(state); }
	static void restore(TransferItemsState& state, int) { restore(state); }
	static void tab(PurchaseState& state, TextButton* sender)
	{
		const int index = tabIndex(sender, state._hdWorkspaceTabs);
		if (index < 0) return;
		snapshot(state); replace(calypsoLogisticsTabAt(static_cast<std::size_t>(index)));
	}
	static void tab(SellState& state, TextButton* sender)
	{
		const int index = tabIndex(sender, state._hdWorkspaceTabs);
		if (index < 0) return;
		snapshot(state); replace(calypsoLogisticsTabAt(static_cast<std::size_t>(index)));
	}
	static void tab(TransferBaseState& state, TextButton* sender)
	{
		const int index = tabIndex(sender, state._hdWorkspaceTabs);
		if (index < 0) return;
		if (state._lstBases && state._lstBases->getSelectedRow() >= 0 &&
			static_cast<std::size_t>(state._lstBases->getSelectedRow()) < state._bases.size())
			rememberDestination(state._bases[state._lstBases->getSelectedRow()]);
		replace(calypsoLogisticsTabAt(static_cast<std::size_t>(index)));
	}
	static void tab(TransferItemsState& state, TextButton* sender)
	{
		const int index = tabIndex(sender, state._hdWorkspaceTabs);
		if (index < 0) return;
		rememberDestination(state._baseTo);
		snapshot(state); replace(calypsoLogisticsTabAt(static_cast<std::size_t>(index)));
	}
	static void clear()
	{
		g_session.clear(); g_game = nullptr; g_base = nullptr; g_destination = nullptr;
	}
};

CalypsoLogisticsWorkspaceSession& calypsoLogisticsWorkspace() { return g_session; }
bool calypsoLogisticsWorkspaceOpen(Game* game, Base* base, CalypsoLogisticsTab tab)
{
	return CalypsoLogisticsWorkspace::open(game, base, tab);
}
void calypsoLogisticsWorkspaceEnsure(Game* game, Base* base, CalypsoLogisticsTab tab)
{ CalypsoLogisticsWorkspace::ensure(game, base, tab); }
void calypsoLogisticsWorkspaceRememberDestination(Base* destination)
{ CalypsoLogisticsWorkspace::rememberDestination(destination); }
void calypsoLogisticsWorkspaceRestoreDestination(TransferBaseState& state)
{ CalypsoLogisticsWorkspace::restoreDestination(state); }
void calypsoLogisticsWorkspaceRestore(PurchaseState& state) { CalypsoLogisticsWorkspace::restore(state, 0); }
void calypsoLogisticsWorkspaceRestore(SellState& state) { CalypsoLogisticsWorkspace::restore(state, 0); }
void calypsoLogisticsWorkspaceRestore(TransferItemsState& state) { CalypsoLogisticsWorkspace::restore(state, 0); }
void calypsoLogisticsWorkspaceTab(PurchaseState& state, TextButton* sender)
{ CalypsoLogisticsWorkspace::tab(state, sender); }
void calypsoLogisticsWorkspaceTab(SellState& state, TextButton* sender)
{ CalypsoLogisticsWorkspace::tab(state, sender); }
void calypsoLogisticsWorkspaceTab(TransferBaseState& state, TextButton* sender)
{ CalypsoLogisticsWorkspace::tab(state, sender); }
void calypsoLogisticsWorkspaceTab(TransferItemsState& state, TextButton* sender)
{ CalypsoLogisticsWorkspace::tab(state, sender); }
void calypsoLogisticsWorkspaceClear() { CalypsoLogisticsWorkspace::clear(); }
}}
#endif
