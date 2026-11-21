#pragma once
#ifdef __EMSCRIPTEN__

#include "CalypsoHdOperationsModel.h"

namespace OpenXcom
{
class TechTreeViewerState;

namespace Calypso
{
class CalypsoHdOperationsRenderer;
class CalypsoHdOperationsChrome;

/// HD presentation of the registered Tech Tree viewer route (re-review P1).
/// The native TechTreeViewerState stays the behavior and data owner; this
/// adapter publishes an authorized two-collection snapshot through the shared
/// operations renderer and never keeps a screen-specific painter.
class CalypsoF15TechTreeUi
{
public:
	explicit CalypsoF15TechTreeUi(TechTreeViewerState *state);
	~CalypsoF15TechTreeUi();

	static void configure(TechTreeViewerState &state);
	static bool resize(TechTreeViewerState &state);

	void refresh();

private:
	void syncGeometry();
	CalypsoHdOperationsModel buildModel() const;
	void applyGeometry();
	CalypsoHdOperationsModel buildViewModel() const;

	TechTreeViewerState *_state = nullptr;
	CalypsoHdOperationsRenderer *_renderer = nullptr;
	CalypsoHdOperationsChrome *_chrome = nullptr;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
