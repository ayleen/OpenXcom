#pragma once
/*
 * Shared full-screen HD renderer. Generated screen contracts are mapped into
 * this archetype-neutral model by the owning state or harness fixture; the
 * renderer never branches on a screen identity and never owns behavior.
 */
#ifdef __EMSCRIPTEN__


#include "CalypsoHdFamilyAdapter.h"

#include "CalypsoHdScreenModel.h"
#include "CalypsoHdFontSource.h"
// Fully defines CalypsoGeoscapeHdRuntimeModel before the by-value
// CalypsoGeoscapeHdSnapshotCache<CalypsoGeoscapeHdRuntimeModel> member below.
#include "CalypsoGeoscapeHdRuntime.h"
#include "CalypsoGeoscapeHdSnapshot.h"

namespace OpenXcom
{
class GeoscapeState;
namespace Calypso
{

class CalypsoHdScreenRenderer : public CalypsoHdFamilyAdapter
{
public:
	CalypsoHdScreenRenderer(const void* state, CalypsoHdScreenRenderModel model,
		CalypsoHdScreenRenderMode mode = CalypsoHdScreenRenderMode::HarnessFullPhysical);
	~CalypsoHdScreenRenderer() override;

	const void* topState() const override;
	bool suppressLogicalState() const override;
	void collectLogicalSuppression(CalypsoHdLogicalSuppression& suppression) const override;
	bool physicalReady() const override;
	bool completeFrameReady() const override;
	bool retryableReadiness() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;
	void setModel(CalypsoHdScreenRenderModel model);

	/// Fail-closed covered-state ownership (rendering contract, 2026-08-25):
	/// while the live strategic chrome is covered by any other state it keeps
	/// feeding its logical suppression list so reprojected legacy controls can
	/// never leak around a blocking modal.
	bool suppressWhenCovered() const override
	{
		return _mode == CalypsoHdScreenRenderMode::GeoscapeLiveChrome;
	}

private:
	static CalypsoGeoscapeHdRuntimeModel liveGeoscapeModel(const GeoscapeState& state);
	static CalypsoGeoscapeHdSnapshotKey liveGeoscapeKey(const GeoscapeState& state);
	const CalypsoGeoscapeHdRuntimeModel& liveGeoscapeSnapshot(const GeoscapeState& state) const;
	bool resolvePhysicalFonts(CalypsoTtfSourceDescriptor& heading,
		CalypsoTtfSourceDescriptor& body, CalypsoTtfSourceDescriptor& mono) const;
	const void* _state;
	CalypsoHdScreenRenderModel _model;
	CalypsoHdScreenRenderMode _mode;
	// State-owned generation-invalidated snapshot (one per registered adapter;
	// dies with the state). Mutable: readiness/collection are const.
	mutable CalypsoGeoscapeHdSnapshotCache<CalypsoGeoscapeHdRuntimeModel> _liveSnapshot;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
