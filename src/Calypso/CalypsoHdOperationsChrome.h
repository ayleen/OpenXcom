#pragma once
#ifdef __EMSCRIPTEN__

#include "CalypsoHdOperationsModel.h"
#include <array>

namespace OpenXcom
{
class Game;
class State;
class TextButton;

namespace Calypso
{
/// Effective viewport used by R&D adapters and the shared native chrome.
/// Side-by-side harness mode owns the left half of the browser canvas; all
/// other presentation modes retain the full frozen viewport.
struct CalypsoHdOperationsEffectiveViewport
{
	int cssWidth = 1;
	int cssHeight = 1;
	int physicalWidth = 1;
	int physicalHeight = 1;
	int projectionLogicalWidth = 1;
	int projectionLogicalHeight = 1;
	bool sideBySide = false;
};

CalypsoHdOperationsEffectiveViewport calypsoHdOperationsEffectiveViewport();

CalypsoHdOperationsRect calypsoHdOperationsProjectForCurrentPresentation(
	const CalypsoHdOperationsRect& rect, int designWidth, int designHeight);

bool calypsoHdOperationsRouteEnabled(const Game* game, const char* familyId);
void calypsoHdOperationsPublishHarnessVisibility();


/// Shared input/data bridge for the immutable F01 header and global rail used
/// by persistent Basescape child screens. The state owns every native button;
/// this holder only retains non-owning pointers and projects their hit regions.
class CalypsoHdOperationsChrome
{
public:
	explicit CalypsoHdOperationsChrome(State& state);

	void applyGeometry() const;
	void populateModel(CalypsoHdOperationsModel& model) const;

private:
	std::array<TextButton*, 7> _buttons{};
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
