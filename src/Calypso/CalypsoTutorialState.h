#ifdef __EMSCRIPTEN__
#pragma once
/*
 * Phase 37.2 (Calypso): tutorial popup state — DOM overlay edition.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * The state machine (queue, step walking, persistence) stays in C++; only the
 * visual rendering moved to a DOM overlay (tutorial-overlay.js).  The C++ side
 * pushes page data to JS via calypso_notify_tutorial_show() and receives
 * button callbacks through the EMSCRIPTEN_KEEPALIVE exports at the bottom of
 * the .cpp.  The state itself is an invisible modal: _screen=false, no child
 * surfaces, so the underlying game frame stays visible behind the HTML popup.
 */

#include <cstddef>
#include <vector>
#include <string>
#include "../Engine/State.h"
#include "CalypsoTutorial.h"  // for CalypsoTutorialStep

namespace OpenXcom
{

class CalypsoTutorialState : public State
{
private:
	std::vector<const CalypsoTutorialStep*> _steps;
	size_t _stepIdx = 0;
	size_t _pageIdx = 0;

	/// Cached translated strings (computed in showPage, sent to JS).
	std::string _curTitle;
	std::string _curBody;
	std::string _curNextLabel;
	int _curPageNum = 1;
	int _curTotalPages = 1;

	/// Singleton-style pointer so JS exports can find the active popup.
	static CalypsoTutorialState* _active;

public:
	explicit CalypsoTutorialState(std::vector<const CalypsoTutorialStep*> steps);
	~CalypsoTutorialState();
	void init() override;

	/// Returns the currently active popup, or nullptr (used by JS exports).
	static CalypsoTutorialState* getActive() { return _active; }

	// The state is invisible — no blit, no think beyond defaults.
	// Input blocking comes from State's modal flag (_screen=false, _modal-driven).

	/// Handler for NEXT / GOT IT (called from JS export).
	void btnNextClick();
	/// Handler for DISABLE HELP (called from JS export).
	void btnStopClick();
	/// Handler for CLOSE / Esc (called from JS export).
	void btnEscClick();

private:
	void showPage();
	void advance();
	void finishCurrentStep();
	const CalypsoTutorialStep* cur() const;
	std::string curAnchorKey() const;
};

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
