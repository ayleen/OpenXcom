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
