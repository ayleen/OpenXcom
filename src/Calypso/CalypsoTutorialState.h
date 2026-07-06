#ifdef __EMSCRIPTEN__
#pragma once
/*
 * Phase 37.2 (Calypso): tutorial popup state.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * Modal, non-fullscreen state (_screen = false, modelled on PauseState).
 * Receives an ordered batch of queued steps from CalypsoTutorial::pump() and
 * walks them page-by-page with a pulsing 2px highlight border around the
 * current page's anchor rect. Slice A ships border-only: no dim overlay
 * (no new alpha/blending path).
 */

#include <vector>
#include "../Engine/State.h"
#include "CalypsoTutorial.h"  // for CalypsoTutorialStep

namespace OpenXcom
{

class Window;
class Text;
class TextButton;
class Surface;
class Action;

class CalypsoTutorialState : public State
{
private:
	std::vector<const CalypsoTutorialStep*> _steps;
	size_t _stepIdx = 0;   ///< current step index into _steps
	size_t _pageIdx = 0;   ///< current page index within the current step
	int    _pulse  = 0;    ///< think() tick counter driving the border pulse

	Window*      _window;
	Text*        _txtTitle;
	Text*        _txtBody;
	TextButton*  _btnNext;
	TextButton*  _btnStop;
	Surface*     _highlight;   ///< full logical-size overlay for the pulsing border

public:
	/// Creates the tutorial popup state with an ordered batch of steps.
	explicit CalypsoTutorialState(std::vector<const CalypsoTutorialStep*> steps);
	/// Cleans up the tutorial popup state.
	~CalypsoTutorialState();
	/// Initializes the state (base init + first page).
	void init() override;
	/// Per-frame: pulse the border.
	void think() override;
	/// Calypso (Emscripten): rescale to the logical buffer instead of the base recenter.
	void resize(int& dX, int& dY) override;

	/// Handler for clicking NEXT / GOT IT.
	void btnNextClick(Action* action);
	/// Handler for clicking STOP SHOWING HINTS.
	void btnStopClick(Action* action);
	/// Handler for Esc — skips the rest of the current step, then advances.
	void btnEscClick(Action* action);

private:
	/// Render the current page's text + button labels into the panel.
	void showPage();
	/// Advance one page (or step), popping the state at the end of the batch.
	void advance();
	/// Mark the current step shown and move on.
	void finishCurrentStep();
	/// Redraw the pulsing border around the current anchor (no-op if no anchor).
	void drawHighlight();
	/// Place the panel in the half of the screen opposite the anchor. Decided
	/// once at construction (pre-scaling) so enableUiScaling stays correct.
	void placeForAnchor();
	/// Returns the current step or nullptr if past the end.
	const CalypsoTutorialStep* cur() const;
	/// Resolves the anchor key for the current page (page override if present).
	std::string curAnchorKey() const;
};

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
