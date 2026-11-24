#pragma once
#ifdef __EMSCRIPTEN__
#include <array>
#include "CalypsoTabbedManagementRenderer.h"

namespace OpenXcom {
class DebriefingState;
class RuleItem;
namespace Calypso {
class CalypsoMissionOutcome final : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoMissionOutcome(DebriefingState &owner);
	~CalypsoMissionOutcome() override;
	const void *topState() const override { return &_state; }
	bool suppressWhenCovered() const override { return true; }
	void collectLogicalSuppression(CalypsoHdLogicalSuppression &suppression) const override;
	void collect(CalypsoHdFrameBuilder &builder) const override;
	void layout();
	void capture();
	void refresh();
	void click(Action *action);
private:
	DebriefingState &_state;
	std::array<TextButton *, 3> _tabs{}, _actions{};
	std::array<TextButton *, 2> _paging{};
	std::array<std::vector<CalypsoTabbedRow>, 3> _rows;
	std::vector<std::pair<const RuleItem *, int>> _initialStock;
	CalypsoLogicalRect _window{};
	int _layout = 0, _tab = 0;
	size_t _page = 0, _evidenceRows = 0;
	std::uint64_t _generation;
	mutable CalypsoSmallConfirmationMotion _motion{};
};
} }
#endif
