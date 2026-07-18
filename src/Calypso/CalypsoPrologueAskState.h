#ifdef __EMSCRIPTEN__
#pragma once
/*
 * Phase 41 (Calypso): "play the prologue mission?" prompt, shown for each
 * fresh New Game whose per-campaign tutorial checkbox is enabled.
 * Whole file Emscripten-only. Structurally cloned from CalypsoTutorialAskState.
 */
#include "../Engine/State.h"

namespace OpenXcom
{
class Window;
class Text;
class TextButton;
class Action;

class CalypsoPrologueAskState : public State
{
private:
	Window*     _window;
	Text*       _txtTitle;
	Text*       _txtBody;
	TextButton* _btnYes;
	TextButton* _btnNo;
public:
	CalypsoPrologueAskState();
	~CalypsoPrologueAskState();
	void resize(int& dX, int& dY) override;
	void btnYesClick(Action* action);
	void btnNoClick(Action* action);
};

} // namespace OpenXcom
#endif // __EMSCRIPTEN__
