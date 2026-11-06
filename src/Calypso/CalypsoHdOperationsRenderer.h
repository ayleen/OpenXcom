#pragma once
/*
 * Shared native HD renderer for operations-workspace, operations-detail, and
 * wide-detail forms.
 * It is deliberately unaware of route/screen identities and receives all
 * geometry from an adapter-populated CalypsoHdOperationsModel.
 */
#include "CalypsoHdOperationsModel.h"
#include <functional>

#ifdef __EMSCRIPTEN__

#include "CalypsoHdFamilyAdapter.h"

namespace OpenXcom
{
namespace Calypso
{

class CalypsoHdOperationsRenderer final : public CalypsoHdFamilyAdapter
{
public:
	CalypsoHdOperationsRenderer(const void* state, CalypsoHdOperationsModel model);
	~CalypsoHdOperationsRenderer() override;

	const void* topState() const override;
	bool suppressLogicalState() const override { return true; }
	void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
	bool physicalReady() const override;
	bool completeFrameReady() const override;
	bool retryableReadiness() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;

	void setModel(CalypsoHdOperationsModel model);
	void setModelProvider(std::function<CalypsoHdOperationsModel()> provider);
	const CalypsoHdOperationsModel& model() const { return _model; }

private:
	bool physicalFontsPresent() const;

	const void* _state;
	mutable CalypsoHdOperationsModel _model;
	std::function<CalypsoHdOperationsModel()> _modelProvider;
};

/// The current operations renderer is published only for the active
/// Emscripten harness snapshot. It is diagnostic state, never a gameplay
/// owner, and may be null during ordinary rendering or teardown.
const CalypsoHdOperationsRenderer* calypsoHdOperationsActiveRenderer();

/// Serialize the already-emitted shared model without invoking an adapter or
/// mutating native state. The harness wraps this model payload with the live
/// save/base snapshot and exposes it through its read-only C export.
std::string calypsoHdOperationsModelSnapshotJson(
	const CalypsoHdOperationsModel& model);

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
