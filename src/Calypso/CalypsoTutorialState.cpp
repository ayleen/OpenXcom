#ifdef __EMSCRIPTEN__
/*
 * Phase 37.2 (Calypso): tutorial popup state — DOM overlay edition.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * The C++ state machine (queue draining, step walking, persistence) is
 * unchanged from Phase 37; only the rendering moved from OXCE bitmap widgets
 * to an HTML DOM overlay (tutorial-overlay.js).  The state is an invisible
 * modal: it blocks game input (State modal flag) but draws no surfaces, so the
 * game frame stays visible behind the popup.
 *
 * Flow:
 *   constructor → init() → showPage() → calypso_notify_tutorial_show(...)
 *   JS button click → EMSCRIPTEN_KEEPALIVE export → btnNextClick/btnStopClick/btnEscClick
 *   advance() → showPage() → calypso_notify_tutorial_show(...) (next page)
 *   or _game->popState() (end of batch) → ~dtor → notifyPopupClosed → JS hide
 */

#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <emscripten.h>

#include "CalypsoTutorialState.h"
#include "CalypsoTutorial.h"

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Logger.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{

/// Static pointer used by the JS callable exports to reach the active popup.
CalypsoTutorialState* CalypsoTutorialState::_active = nullptr;

/* ── JS bridge ────────────────────────────────────────────────────────── */

static void calypso_notify_tutorial_show(
	int pageNum, int totalPages,
	const std::string& title, const std::string& body,
	const std::string& nextLabel,
	bool isLastStep)
{
	EM_ASM_({
		if (globalThis.__calypsoTutorialShow)
			globalThis.__calypsoTutorialShow(
				$0, $1,
				UTF8ToString($2), UTF8ToString($3),
				UTF8ToString($4), $5);
	}, pageNum, totalPages,
	   title.c_str(), body.c_str(),
	   nextLabel.c_str(), isLastStep ? 1 : 0);
}

static void calypso_notify_tutorial_hide()
{
	EM_ASM_({
		if (globalThis.__calypsoTutorialHide)
			globalThis.__calypsoTutorialHide();
	});
}

/* ── State ────────────────────────────────────────────────────────────── */

CalypsoTutorialState::CalypsoTutorialState(std::vector<const CalypsoTutorialStep*> steps)
	: _steps(std::move(steps))
{
	// _screen=false → non-fullscreen modal state (like PauseState).
	// No surfaces are added — the popup is pure DOM overlay.
	_screen = false;
	_active = this;
}

CalypsoTutorialState::~CalypsoTutorialState()
{
	// Hide the DOM overlay in case the state was popped without an explicit
	// close (e.g. load-game, state-stack reset).
	calypso_notify_tutorial_hide();
	CalypsoTutorial::get().notifyPopupClosed();
	_active = nullptr;
}

void CalypsoTutorialState::init()
{
	State::init();
	showPage();
}

void CalypsoTutorialState::showPage()
{
	const CalypsoTutorialStep* s = cur();
	if (!s)
	{
		calypso_notify_tutorial_hide();
		_game->popState();
		return;
	}

	// Resolve translated strings through the engine's i18n.
	_curBody = std::string(tr(s->pages[_pageIdx]));

	std::ostringstream titleStream;
	titleStream << std::string(tr("STR_CAL_TUT_TITLE")) << " " << (_pageIdx + 1) << "/" << s->pages.size();
	_curTitle = titleStream.str();

	bool lastPageOfLastStep = (_stepIdx + 1 >= _steps.size()) && (_pageIdx + 1 >= s->pages.size());
	_curNextLabel = std::string(tr(lastPageOfLastStep ? "STR_CAL_TUT_GOTIT" : "STR_CAL_TUT_NEXT"));

	_curPageNum = static_cast<int>(_pageIdx + 1);
	_curTotalPages = static_cast<int>(s->pages.size());

	calypso_notify_tutorial_show(
		_curPageNum, _curTotalPages,
		_curTitle, _curBody,
		_curNextLabel,
		lastPageOfLastStep);
}

void CalypsoTutorialState::advance()
{
	const CalypsoTutorialStep* s = cur();
	if (s && _pageIdx + 1 < s->pages.size())
	{
		_pageIdx++;
		showPage();
		return;
	}
	finishCurrentStep();
	_stepIdx++;
	_pageIdx = 0;
	if (_stepIdx >= _steps.size())
	{
		calypso_notify_tutorial_hide();
		_game->popState();
		return;
	}
	showPage();
}

void CalypsoTutorialState::finishCurrentStep()
{
	if (const CalypsoTutorialStep* s = cur())
		CalypsoTutorial::get().markShown(s->id);
}

void CalypsoTutorialState::btnNextClick() { advance(); }

void CalypsoTutorialState::btnEscClick()
{
	calypso_notify_tutorial_hide();
	finishCurrentStep();
	advance();
}

void CalypsoTutorialState::btnStopClick()
{
	calypso_notify_tutorial_hide();
	CalypsoTutorial::get().disableForCampaign();
	_game->popState();
}

const CalypsoTutorialStep* CalypsoTutorialState::cur() const
{
	return _stepIdx < _steps.size() ? _steps[_stepIdx] : nullptr;
}

} // namespace OpenXcom

/* ── JS callable exports ──────────────────────────────────────────────── */

extern "C" {

EMSCRIPTEN_KEEPALIVE
void calypso_tutorial_advance()
{
	if (auto* t = OpenXcom::CalypsoTutorialState::getActive())
		t->btnNextClick();
}

EMSCRIPTEN_KEEPALIVE
void calypso_tutorial_stop()
{
	if (auto* t = OpenXcom::CalypsoTutorialState::getActive())
		t->btnStopClick();
}

EMSCRIPTEN_KEEPALIVE
void calypso_tutorial_close()
{
	if (auto* t = OpenXcom::CalypsoTutorialState::getActive())
		t->btnEscClick();
}

} /* extern "C" */

#endif // __EMSCRIPTEN__
