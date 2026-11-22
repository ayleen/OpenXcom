#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoMissionReview.h"
namespace OpenXcom {
class ConfirmLandingState;
namespace Calypso {
class CalypsoF20ConfirmLandingUi : public CalypsoMissionReview
{
public:
	using CalypsoMissionReview::CalypsoMissionReview;
	static void configure(ConfirmLandingState &state, bool allow = true);
	static bool resize(ConfirmLandingState &state);
};
} }
#endif
