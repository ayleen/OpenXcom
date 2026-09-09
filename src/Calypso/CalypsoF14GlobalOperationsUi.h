#pragma once

#ifdef __EMSCRIPTEN__

#include "CalypsoHdOperationsModel.h"

namespace OpenXcom
{
class GlobalManufactureState;
class GlobalResearchDiaryState;
class GlobalResearchState;

namespace Calypso
{
class CalypsoHdOperationsRenderer;
class CalypsoHdOperationsChrome;

/// F14 bridge for the three global operations overview states.
/// All presentation is delegated to CalypsoHdOperationsRenderer; this class
/// only projects generated geometry and live native state into its model.
class CalypsoF14GlobalOperationsUi
{
public:
	explicit CalypsoF14GlobalOperationsUi(GlobalResearchState *state);
	explicit CalypsoF14GlobalOperationsUi(GlobalResearchDiaryState *state);
	explicit CalypsoF14GlobalOperationsUi(GlobalManufactureState *state);
	~CalypsoF14GlobalOperationsUi();

	static void configure(GlobalResearchState &state);
	static void configure(GlobalResearchDiaryState &state);
	static void configure(GlobalManufactureState &state);
	static bool resize(GlobalResearchState &state);
	static bool resize(GlobalResearchDiaryState &state);
	static bool resize(GlobalManufactureState &state);

	void refresh();

private:
	enum class Kind
	{
		Research,
		Diary,
		Manufacture
	};

	void syncGeometry();
	void ensureResearchOwners();
	void ensureDiaryOwners();
	void ensureManufactureOwners();
	void applyResearchGeometry();
	void applyDiaryGeometry();
	void applyManufactureGeometry();
	CalypsoHdOperationsModel buildModel() const;
	CalypsoHdOperationsModel buildResearchModel() const;
	CalypsoHdOperationsModel buildDiaryModel() const;
	CalypsoHdOperationsModel buildManufactureModel() const;

	Kind _kind;
	GlobalResearchState *_research = nullptr;
	GlobalResearchDiaryState *_diary = nullptr;
	GlobalManufactureState *_manufacture = nullptr;
	CalypsoHdOperationsRenderer *_renderer = nullptr;
	CalypsoHdOperationsChrome *_chrome = nullptr;
};
} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__