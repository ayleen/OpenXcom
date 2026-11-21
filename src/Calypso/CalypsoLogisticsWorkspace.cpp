
#ifdef __EMSCRIPTEN__
#include "CalypsoLogisticsWorkspace.h"
#include "../Menu/ErrorMessageState.h"
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
#include "../Savegame/Soldier.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleSoldier.h"
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

std::string identity(const TransferRow& row, CalypsoLogisticsTab tab)
{
	const std::string type = std::to_string(static_cast<int>(row.type)) + ":";
	switch (row.type)
	{
	case TRANSFER_ITEM:
		return type + static_cast<const RuleItem*>(row.rule)->getType();
	case TRANSFER_SOLDIER:
		return type + (tab == CalypsoLogisticsTab::Purchase
			? static_cast<const RuleSoldier*>(row.rule)->getType()
			: std::to_string(static_cast<const Soldier*>(row.rule)->getId()));
	case TRANSFER_CRAFT:
		return type + (tab == CalypsoLogisticsTab::Purchase
			? static_cast<const RuleCraft*>(row.rule)->getType()
			: static_cast<const Craft*>(row.rule)->getRules()->getType() + ":" +
				std::to_string(static_cast<const Craft*>(row.rule)->getId()));
	default:
		return type;
	}
}

std::vector<CalypsoLogisticsDraftRow> draftRows(const std::vector<TransferRow>& rows,
	CalypsoLogisticsTab tab)
{
	std::vector<CalypsoLogisticsDraftRow> result;
	for (const auto& row : rows)
		if (row.amount > 0) result.push_back({identity(row, tab), row.amount});
	return result;
}

bool restoreRows(std::vector<TransferRow>& rows, const CalypsoLogisticsDraft& draft)
{
	if (!draft.hasDraft) return false;
	std::map<std::string, int> amounts;
	for (const auto& saved : draft.rows) amounts.emplace(saved.identity, saved.amount);
	std::vector<int> selected;
	selected.reserve(rows.size());
	for (const auto& row : rows)
	{
		const auto it = amounts.find(identity(row, draft.tab));
		if (it == amounts.end()) { selected.push_back(0); continue; }
		if (draft.tab != CalypsoLogisticsTab::Purchase && it->second > row.qtySrc)
			return false;
		selected.push_back(it->second);
		amounts.erase(it);
	}
	if (!amounts.empty()) return false;
	for (std::size_t i = 0; i < rows.size(); ++i) rows[i].amount = selected[i];
	return true;
}

}

class CalypsoLogisticsWorkspace
{
	static void snapshot(PurchaseState& state)
	{
		const auto tab = CalypsoLogisticsTab::Purchase;
		CalypsoLogisticsDraft draft;
		draft.tab = tab;
		draft.rows = draftRows(state._items, tab);
		draft.counterparty = state._counterparty;
		draft.total = state._total;
		draft.personnel = state._pQty;
		draft.craft = state._cQty;
		draft.storage = state._iQty;
		draft.prison = state._iPrisonQty;
		g_session.drafts().snapshot(std::move(draft));
	}
	static void snapshot(SellState& state)
	{
		const auto tab = CalypsoLogisticsTab::Sell;
		CalypsoLogisticsDraft draft;
		draft.tab = tab;
		draft.rows = draftRows(state._items, tab);
		draft.counterparty = state._counterparty;
		draft.total = state._total;
		draft.storage = state._spaceChange;
		g_session.drafts().snapshot(std::move(draft));
	}
	static void snapshot(TransferItemsState& state)
	{
		const auto tab = CalypsoLogisticsTab::Transfer;
		CalypsoLogisticsDraft draft;
		draft.tab = tab;
		draft.rows = draftRows(state._items, tab);
		draft.total = state._total;
		draft.personnel = state._pQty;
		draft.craft = state._cQty;
		draft.aliens = state._aQty;
		draft.storage = state._iQty;
		g_session.drafts().snapshot(std::move(draft));
	}
	template<typename State>
	static void refreshDraft(State& state)
	{
		state.updateList();
		if (state._sel < state._rows.size()) state.updateItemStrings();
	}
	static void restore(PurchaseState& state)
	{
		const auto& draft = g_session.drafts().get(CalypsoLogisticsTab::Purchase);
		if (draft.counterparty != state._counterparty || !restoreRows(state._items, draft)) return;
		state._total = static_cast<int>(draft.total);
		state._pQty = draft.personnel;
		state._cQty = draft.craft;
		state._iQty = draft.storage;
		state._iPrisonQty = draft.prison;
		refreshDraft(state);
	}
	static void restore(SellState& state)
	{
		const auto& draft = g_session.drafts().get(CalypsoLogisticsTab::Sell);
		if (draft.counterparty != state._counterparty || !restoreRows(state._items, draft)) return;
		state._total = draft.total;
		state._spaceChange = draft.storage;
		refreshDraft(state);
	}
	static void restore(TransferItemsState& state)
	{
		const auto& draft = g_session.drafts().get(CalypsoLogisticsTab::Transfer);
		if (g_destination != state._baseTo || !restoreRows(state._items, draft)) return;
		state._total = static_cast<int>(draft.total);
		state._pQty = draft.personnel;
		state._cQty = draft.craft;
		state._aQty = draft.aliens;
		state._iQty = draft.storage;
		refreshDraft(state);
	}
	static int tabIndex(TextButton* sender, TextButton* const (&tabs)[3])
	{
		for (int i = 0; i < 3; ++i) if (tabs[i] == sender) return i;
		return -1;
	}
	static void push(CalypsoLogisticsTab tab)
	{
		const auto& draft = g_session.drafts().get(tab);
		if (tab != CalypsoLogisticsTab::Transfer && draft.counterparty.empty() &&
			g_game->getSavedGame() && g_game->getSavedGame()->getCalypsoEconomy() &&
			g_game->getSavedGame()->getCalypsoEconomy()->active())
		{
			g_game->pushState(new CalypsoMarketState(g_base, tab == CalypsoLogisticsTab::Sell));
			return;
		}
		switch (tab)
		{
		case CalypsoLogisticsTab::Purchase: g_game->pushState(new PurchaseState(g_base, nullptr, draft.counterparty)); break;
		case CalypsoLogisticsTab::Sell: g_game->pushState(new SellState(g_base, nullptr, OPT_GEOSCAPE, draft.counterparty)); break;
		case CalypsoLogisticsTab::Transfer: g_game->pushState(new TransferBaseState(g_base, nullptr)); break;
		default: break;
		}
	}
	static void replace(CalypsoLogisticsTab tab)
	{
		if (!g_game || !g_base || tab == g_session.activeTab() || !g_session.switchTo(tab)) return;
		g_game->popState();
		// The market picker and transfer destination belong to the tab we left.
		State* parent = g_game->getTopState();
		if (dynamic_cast<CalypsoMarketState*>(parent) || dynamic_cast<TransferBaseState*>(parent))
			g_game->popState();
		push(tab);
	}
public:
	static bool open(Game* game, Base* base, CalypsoLogisticsTab tab)
	{
		if (!game || !base || tab == CalypsoLogisticsTab::Unknown) return false;
		g_game = game; g_base = base; g_destination = nullptr;
		g_session.begin(tab);
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
		if (destination != g_destination) g_session.drafts().clear(CalypsoLogisticsTab::Transfer);
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

ErrorMessageHdForm calypsoLogisticsErrorForm(const State* owner, const std::string& section,
	const std::string& message, const std::string& ok)
{
	ErrorMessageHdForm form;
	if (!owner) return form;
	form.hdUnderlayState = owner;
	form.protocol = section;
	form.actionLabel = ok;
	const auto separator = message.find_first_of("\n\x02");
	if (separator != std::string::npos && separator > 0 && separator + 1 < message.size())
	{
		form.title = message.substr(0, separator);
		form.bodyLines.push_back(message.substr(separator + 1));
	}
	else
	{
		form.title = section;
		form.bodyLines.push_back(message);
	}
	return form;
}

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
