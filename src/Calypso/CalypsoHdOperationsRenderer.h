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

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
