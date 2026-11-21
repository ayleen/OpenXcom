#pragma once
#ifdef __EMSCRIPTEN__

#include <cstddef>
#include "CalypsoHdOperationsModel.h"

namespace OpenXcom
{
class ResearchState;
class NewResearchListState;
class ResearchInfoState;
class TextButton;

namespace Calypso
{
class CalypsoHdOperationsRenderer;
class CalypsoHdOperationsChrome;

class CalypsoF09ResearchUi
{
public:
	explicit CalypsoF09ResearchUi(ResearchState *state);
	explicit CalypsoF09ResearchUi(NewResearchListState *state);
	explicit CalypsoF09ResearchUi(ResearchInfoState *state);
	~CalypsoF09ResearchUi();

	static void configure(ResearchState &state);
	static void configure(NewResearchListState &state);
	static void configure(ResearchInfoState &state);
	static bool resize(ResearchState &state);
	static bool resize(NewResearchListState &state);
	static bool resize(ResearchInfoState &state);
	/// Queue the one-shot L04 setup before the target state is constructed.
	static void queueHarnessQueueListSetup(size_t selectedRow, size_t scrollOffset);
	/// Queue a real catalogue selection before the first HD snapshot. The
	/// browser must not synthesize a selected row for filter/sort cases.
	static void queueHarnessCatalogueListSetup(size_t selectedRow);
	static void clearHarnessQueueListSetup();

	void refresh();
	bool ownsQueueReorderPoint(int x, int y) const;

private:
	enum class Kind { Queue, Catalogue, Staffing };

	void syncGeometry();
	void applyQueueGeometry();
	void applyCatalogueGeometry();
	void applyStaffingGeometry();
	CalypsoHdOperationsModel buildModel() const;
	CalypsoHdOperationsModel buildQueueModel() const;
	CalypsoHdOperationsModel buildCatalogueModel() const;
	CalypsoHdOperationsModel buildStaffingModel() const;
	void ensureQueueOwners();
	void ensureCatalogueOwners();
	void ensureStaffingOwners();

	Kind _kind;
	ResearchState *_queue = nullptr;
	NewResearchListState *_catalogue = nullptr;
	ResearchInfoState *_staffing = nullptr;
	CalypsoHdOperationsRenderer *_renderer = nullptr;
	CalypsoHdOperationsChrome *_chrome = nullptr;
	TextButton *_moveUp = nullptr;
	TextButton *_moveDown = nullptr;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
