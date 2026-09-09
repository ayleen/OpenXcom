#pragma once
#ifdef __EMSCRIPTEN__

#include "CalypsoHdOperationsModel.h"

namespace OpenXcom
{
class ResearchState;
class NewResearchListState;
class ResearchInfoState;

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

	void refresh();

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
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__