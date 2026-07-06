#ifdef __EMSCRIPTEN__
#pragma once
/*
 * Phase 39 (Calypso): first-run "enable tutorial?" prompt.
 * Whole file Emscripten-only. Modal YES/NO popup shown once on the first
 * Geoscape frame of a NEW campaign (pushed by CalypsoTutorial::pump()).
 * YES keeps the tutorial on; NO disables it for this campaign.
 * Structurally cloned from CalypsoTutorialState.
 */
#include "../Engine/State.h"

namespace OpenXcom
{
class Window;
class Text;
class TextButton;
class Action;

class CalypsoTutorialAskState : public State
{
private:
	Window*     _window;
	Text*       _txtTitle;
	Text*       _txtBody;
	TextButton* _btnYes;
	TextButton* _btnNo;
public:
	CalypsoTutorialAskState();
	~CalypsoTutorialAskState();
	void resize(int& dX, int& dY) override;
	void btnYesClick(Action* action);
	void btnNoClick(Action* action);
};

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
