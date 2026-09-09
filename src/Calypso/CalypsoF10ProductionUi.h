#pragma once
#ifdef __EMSCRIPTEN__

#include "CalypsoHdOperationsModel.h"

namespace OpenXcom
{
class ManufactureState;
class NewManufactureListState;
class ManufactureStartState;
class ManufactureInfoState;
class ManufactureDependenciesTreeState;

namespace Calypso
{
class CalypsoHdOperationsRenderer;

class CalypsoF10ProductionUi
{
public:
	explicit CalypsoF10ProductionUi(ManufactureState *state);
	explicit CalypsoF10ProductionUi(NewManufactureListState *state);
	explicit CalypsoF10ProductionUi(ManufactureStartState *state);
	explicit CalypsoF10ProductionUi(ManufactureInfoState *state);
	explicit CalypsoF10ProductionUi(ManufactureDependenciesTreeState *state);
	~CalypsoF10ProductionUi();

	static void configure(ManufactureState &state);
	static void configure(NewManufactureListState &state);
	static void configure(ManufactureStartState &state);
	static void configure(ManufactureInfoState &state);
	static void configure(ManufactureDependenciesTreeState &state);
	static bool resize(ManufactureState &state);
	static bool resize(NewManufactureListState &state);
	static bool resize(ManufactureStartState &state);
	static bool resize(ManufactureInfoState &state);
	static bool resize(ManufactureDependenciesTreeState &state);

	void refresh();

private:
	enum class Kind { Queue, Catalogue, Requirements, Controls, Dependencies };
	CalypsoHdOperationsModel buildModel() const;
	void applyQueueGeometry();
	void applyCatalogueGeometry();
	void applyRequirementsGeometry();
	void applyControlsGeometry();
	void applyDependenciesGeometry();
	CalypsoHdOperationsModel buildQueueModel() const;
	CalypsoHdOperationsModel buildCatalogueModel() const;
	CalypsoHdOperationsModel buildRequirementsModel() const;
	CalypsoHdOperationsModel buildControlsModel() const;
	CalypsoHdOperationsModel buildDependenciesModel() const;
	void ensureQueueOwners();
	void ensureCatalogueOwners();

	Kind _kind;
	ManufactureState *_queue = nullptr;
	NewManufactureListState *_catalogue = nullptr;
	ManufactureStartState *_requirements = nullptr;
	ManufactureInfoState *_controls = nullptr;
	ManufactureDependenciesTreeState *_dependencies = nullptr;
	CalypsoHdOperationsRenderer *_renderer = nullptr;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__