#pragma once
/*
 * Command Center -- pure interaction contract.
 *
 * Keeps visible rail semantics and base-selector state independent from SDL so
 * native tests can pin the behavior used by the Emscripten interaction bridge.
 */
#include <cstddef>

#include "CommandCenterTypes.h"

namespace OpenXcom
{
namespace Calypso
{
namespace CommandCenter
{

enum class RailAction
{
	World,
	Bases,
	Intercept,
	Graphs,
	Ufopaedia,
};

inline constexpr RailAction railActionForSlot(int slot)
{
	switch (slot)
	{
		case 1: return RailAction::Bases;
		case 2: return RailAction::Intercept; // visible Operations category
		case 3: return RailAction::Graphs;    // visible Analytics category
		case 4: return RailAction::Ufopaedia;
		default: return RailAction::World;
	}
}

/// Authored rail item rect shared by the chrome painter and native input
/// owners (T15): 72px items from rail top + 20px at a 76px stride; Settings
/// pinned 56px above the rail bottom. Single definition site by design.
inline RectF calypsoCcRailItemRect(const RectF& rail, int index)
{
	return { rail.x + 8.0f, rail.y + 20.0f + 76.0f * index, rail.width - 16.0f, 72.0f };
}

inline RectF calypsoCcRailSettingsRect(const RectF& rail)
{
	return { rail.x + 24.0f, rail.bottom() - 56.0f, 40.0f, 40.0f };
}

inline constexpr const char* nativeWidgetForRailAction(RailAction action)
{
	switch (action)
	{
		case RailAction::Bases: return "btnBases";
		case RailAction::Intercept: return "btnIntercept";
		case RailAction::Graphs: return "btnGraphs";
		case RailAction::Ufopaedia: return "btnUfopaedia";
		case RailAction::World: return nullptr;
	}
	return nullptr;
}

class BaseSelectorModel
{
public:
	void reconcile(std::size_t baseCount)
	{
		if (baseCount == 0)
		{
			_selectedIndex = 0;
			_pendingFocusIndex = NoPendingFocus;
			_open = false;
		}
		else
		{
			if (_selectedIndex >= baseCount)
				_selectedIndex = 0;
			if (_pendingFocusIndex >= baseCount)
				_pendingFocusIndex = NoPendingFocus;
		}
	}

	void toggle(std::size_t baseCount)
	{
		reconcile(baseCount);
		_open = baseCount > 0 && !_open;
	}

	bool select(std::size_t index, std::size_t baseCount)
	{
		reconcile(baseCount);
		if (index >= baseCount) return false;
		_selectedIndex = index;
		_pendingFocusIndex = index;
		_open = false;
		return true;
	}

	bool takePendingFocus(std::size_t& index)
	{
		if (_pendingFocusIndex == NoPendingFocus)
			return false;
		index = _pendingFocusIndex;
		_pendingFocusIndex = NoPendingFocus;
		return true;
	}

	void close() { _open = false; }
	bool open() const { return _open; }
	std::size_t selectedIndex() const { return _selectedIndex; }

private:
	static constexpr std::size_t NoPendingFocus = static_cast<std::size_t>(-1);
	std::size_t _selectedIndex = 0;
	std::size_t _pendingFocusIndex = NoPendingFocus;
	bool _open = false;
};

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom
