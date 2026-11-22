#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFamilyAdapter.h"

namespace OpenXcom {
class SelectDestinationState;
namespace Calypso {

class CalypsoFleetMapUi : public CalypsoHdFamilyAdapter
{
public:
	explicit CalypsoFleetMapUi(SelectDestinationState *state) : _state(state) {}
	~CalypsoFleetMapUi() override;
	const void *topState() const override;
	void collect(CalypsoHdFrameBuilder &builder) const override;
	void layout();

private:
	SelectDestinationState *_state;
	bool _wide = false;
};
} }
#endif
