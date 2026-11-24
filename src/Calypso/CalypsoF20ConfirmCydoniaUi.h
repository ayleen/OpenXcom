#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoMissionReview.h"
namespace OpenXcom {
class ConfirmCydoniaState;
namespace Calypso {
class CalypsoF20ConfirmCydoniaUi : public CalypsoMissionReview
{
public:
	using CalypsoMissionReview::CalypsoMissionReview;
	static void configure(ConfirmCydoniaState &state, bool allow = true);
	static bool resize(ConfirmCydoniaState &state);
};
} }
#endif
