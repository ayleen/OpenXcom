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

namespace OpenXcom
{
class GeoscapeState;
namespace Calypso
{

struct CalypsoGeoscapeHdRuntimeModel;


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

private:
	static CalypsoGeoscapeHdRuntimeModel liveGeoscapeModel(const GeoscapeState& state);
	bool resolvePhysicalFonts(CalypsoTtfSourceDescriptor& heading,
		CalypsoTtfSourceDescriptor& body, CalypsoTtfSourceDescriptor& mono) const;
	const void* _state;
	CalypsoHdScreenRenderModel _model;
	CalypsoHdScreenRenderMode _mode;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
