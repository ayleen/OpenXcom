#pragma once
#ifdef __EMSCRIPTEN__
#include <array>
#include "CalypsoTabbedManagementRenderer.h"

namespace OpenXcom
{
class GraphsState;
namespace Calypso
{
class CalypsoStrategicGraphs final : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoStrategicGraphs(GraphsState &state);
	~CalypsoStrategicGraphs() override;
	const void *topState() const override;
	bool suppressWhenCovered() const override { return true; }
	void collectLogicalSuppression(CalypsoHdLogicalSuppression &suppression) const override;
	void collect(CalypsoHdFrameBuilder &builder) const override;
	void layout();
	void click(Action *action);
private:
	GraphsState &_state;
	std::array<TextButton *, 6> _tabs{};
	std::array<TextButton *, 3> _toolbar{}, _actions{};
	std::array<TextButton *, 5> _rows{};
	size_t _page = 0;
	int _mode = 0, _layout = 0;
	bool _legend = false;
	CalypsoLogicalRect _window{};
	mutable CalypsoSmallConfirmationMotion _motion{};
	mutable CalypsoHdImageDescriptor _plot;
	mutable unsigned _revision = 0;
	void updatePlot(int width, int height) const;
};
}
}
#endif
