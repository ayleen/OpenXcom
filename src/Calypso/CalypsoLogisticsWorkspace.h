#pragma once
/*
 * Shared Logistics workspace session.  Native states remain the gameplay
 * owners; this controller only snapshots draft quantities and routes tabs.
 */
#include <cstddef>
#include <string>
#include <vector>

namespace OpenXcom
{
class Action;
class Base;
class BasescapeState;
class Game;
class PurchaseState;
class SellState;
class TextButton;
class TransferBaseState;
class TransferItemsState;

namespace Calypso
{

enum class CalypsoLogisticsTab
{
	Unknown,
	Purchase,
	Sell,
	Transfer
};

inline CalypsoLogisticsTab calypsoLogisticsTabFromId(const std::string& id)
{
	if (id == "purchase") return CalypsoLogisticsTab::Purchase;
	if (id == "sell") return CalypsoLogisticsTab::Sell;
	if (id == "transfer") return CalypsoLogisticsTab::Transfer;
	return CalypsoLogisticsTab::Unknown;
}

inline const char* calypsoLogisticsTabId(CalypsoLogisticsTab tab)
{
	switch (tab)
	{
	case CalypsoLogisticsTab::Purchase: return "purchase";
	case CalypsoLogisticsTab::Sell: return "sell";
	case CalypsoLogisticsTab::Transfer: return "transfer";
	default: return "";
	}
}

inline CalypsoLogisticsTab calypsoLogisticsTabAt(std::size_t index)
{
	switch (index)
	{
	case 0: return CalypsoLogisticsTab::Purchase;
	case 1: return CalypsoLogisticsTab::Sell;
	case 2: return CalypsoLogisticsTab::Transfer;
	default: return CalypsoLogisticsTab::Unknown;
	}
}

struct CalypsoLogisticsDraftRow
{
	std::string identity;
	int amount = 0;
};

struct CalypsoLogisticsDraft
{
	CalypsoLogisticsTab tab = CalypsoLogisticsTab::Unknown;
	std::string label;
	std::string value;
	bool hasDraft = false;
	std::vector<CalypsoLogisticsDraftRow> rows;
};

struct CalypsoLogisticsDraftStore
{
	CalypsoLogisticsDraft drafts[3];

	void set(CalypsoLogisticsTab tab, const std::string& label,
		const std::string& value)
	{
		for (std::size_t i = 0; i < 3; ++i)
		{
			if (calypsoLogisticsTabAt(i) == tab)
			{
				drafts[i].tab = tab;
				drafts[i].label = label;
				drafts[i].value = value;
				drafts[i].hasDraft = true;
				return;
			}
		}
	}

	void snapshot(CalypsoLogisticsTab tab,
		const std::vector<CalypsoLogisticsDraftRow>& rows)
	{
		for (std::size_t i = 0; i < 3; ++i)
		{
			if (calypsoLogisticsTabAt(i) == tab)
			{
				drafts[i].tab = tab;
				drafts[i].rows = rows;
				drafts[i].hasDraft = true;
				return;
			}
		}
	}

	void clear(CalypsoLogisticsTab tab)
	{
		for (std::size_t i = 0; i < 3; ++i)
		{
			if (calypsoLogisticsTabAt(i) == tab)
			{
				drafts[i] = CalypsoLogisticsDraft{};
				drafts[i].tab = tab;
				return;
			}
		}
	}

	const CalypsoLogisticsDraft& get(CalypsoLogisticsTab tab) const
	{
		for (std::size_t i = 0; i < 3; ++i)
			if (calypsoLogisticsTabAt(i) == tab) return drafts[i];
		return drafts[0];
	}

	std::size_t activeDraftCount() const
	{
		std::size_t count = 0;
		for (const auto& draft : drafts)
			if (draft.hasDraft) ++count;
		return count;
	}
};

class CalypsoLogisticsWorkspaceSession
{
	CalypsoLogisticsTab _active = CalypsoLogisticsTab::Unknown;
	std::string _destination;
	CalypsoLogisticsDraftStore _drafts;
public:
	bool begin(CalypsoLogisticsTab initialTab,
		const std::string& destination = std::string())
	{
		if (initialTab == CalypsoLogisticsTab::Unknown) return false;
		_active = initialTab;
		_destination = destination;
		_drafts = CalypsoLogisticsDraftStore{};
		return true;
	}
	bool switchTo(CalypsoLogisticsTab tab)
	{
		if (tab == CalypsoLogisticsTab::Unknown) return false;
		_active = tab;
		return true;
	}
	void clear()
	{
		_active = CalypsoLogisticsTab::Unknown;
		_destination.clear();
		_drafts = CalypsoLogisticsDraftStore{};
	}
	bool active() const { return _active != CalypsoLogisticsTab::Unknown; }
	CalypsoLogisticsTab activeTab() const { return _active; }
	void rememberDestination(const std::string& destination) { _destination = destination; }
	const std::string& destination() const { return _destination; }
	CalypsoLogisticsDraftStore& drafts() { return _drafts; }
	const CalypsoLogisticsDraftStore& drafts() const { return _drafts; }
};

#ifdef __EMSCRIPTEN__

CalypsoLogisticsWorkspaceSession& calypsoLogisticsWorkspace();
bool calypsoLogisticsWorkspaceOpen(Game* game, Base* base,
	CalypsoLogisticsTab initialTab);
void calypsoLogisticsWorkspaceEnsure(Game* game, Base* base,
	CalypsoLogisticsTab tab);
void calypsoLogisticsWorkspaceRestore(PurchaseState& state);
void calypsoLogisticsWorkspaceRestore(SellState& state);
void calypsoLogisticsWorkspaceRestore(TransferItemsState& state);
void calypsoLogisticsWorkspaceRestoreDestination(TransferBaseState& state);
void calypsoLogisticsWorkspaceRememberDestination(Base* destination);
void calypsoLogisticsWorkspaceTab(PurchaseState& state, TextButton* sender);
void calypsoLogisticsWorkspaceTab(SellState& state, TextButton* sender);
void calypsoLogisticsWorkspaceTab(TransferBaseState& state, TextButton* sender);
void calypsoLogisticsWorkspaceTab(TransferItemsState& state, TextButton* sender);
void calypsoLogisticsWorkspaceClear();

#endif

} // namespace Calypso
} // namespace OpenXcom
