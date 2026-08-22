#pragma once
/*
 * Shared full-screen HD renderer. Generated screen contracts are mapped into
 * this archetype-neutral model by the owning state or harness fixture; the
 * renderer never branches on a screen identity and never owns behavior.
 */
#ifdef __EMSCRIPTEN__


#include "CalypsoHdFamilyAdapter.h"

#include "CalypsoHdScreenModel.h"

namespace OpenXcom
{
namespace Calypso
{


class CalypsoHdScreenRenderer : public CalypsoHdFamilyAdapter
{
public:
	CalypsoHdScreenRenderer(const void* state, CalypsoHdScreenRenderModel model);
	~CalypsoHdScreenRenderer() override;

	const void* topState() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;
	void setModel(CalypsoHdScreenRenderModel model);

private:
	const void* _state;
	CalypsoHdScreenRenderModel _model;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__