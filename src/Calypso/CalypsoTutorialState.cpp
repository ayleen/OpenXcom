#ifdef __EMSCRIPTEN__
/*
 * Phase 37.2 (Calypso): tutorial popup state implementation.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * Construction / scaling modelled exactly on src/Menu/PauseState.cpp:
 *   setInterface("pauseMenu") + add(*, id, "pauseMenu"), centerAllSurfaces(),
 *   enableUiScaling(320, 200, 0.75f),
 *   applyTTFToTexts(getTTFFont("FONT_HD_HUD", false), 0.92f),
 *   setWindowBackground, applyBattlescapeTheme, resize() override.
 *
 * Slice A: pulsing 2px highlight border ONLY — no dim overlay (no new
 * alpha/blending path). Anchors are value-copied SDL_Rects held by the
 * CalypsoTutorial manager; a missing anchor simply means "no border".
 */

#include <climits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "CalypsoTutorialState.h"
#include "CalypsoTutorial.h"

#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Builds the tutorial popup. @param steps ordered batch of steps to walk
 * page-by-page; the popup pops itself when the batch is exhausted.
 */
CalypsoTutorialState::CalypsoTutorialState(std::vector<const CalypsoTutorialStep*> steps)
	: _steps(std::move(steps)), _window(nullptr), _txtTitle(nullptr),
	  _txtBody(nullptr), _btnNext(nullptr), _btnStop(nullptr), _highlight(nullptr)
{
	_screen = false;

	// Create objects — highlight first so it sits behind the window (add() order
	// is ascending Z). Sizes/positions are in 320x200 design space.
	_highlight = new Surface(320, 200, 0, 0);
	_window    = new Window(this, 240, 72, 40, 124, POPUP_BOTH);
	_txtTitle  = new Text(228, 16, 46, 130);
	_txtBody   = new Text(228, 40, 46, 148);
	_btnNext   = new TextButton(96, 16, 178, 176);
	_btnStop   = new TextButton(120, 16, 46, 176);

	// Set palette (pauseMenu gives us the in-battle popup palette/colors).
	setInterface("pauseMenu", false, _game->getSavedGame() ? _game->getSavedGame()->getSavedBattle() : 0);

	// add(Surface*) propagates _palette to the surface, so the raw highlight
	// surface picks up the pauseMenu palette automatically — no manual setPalette.
	add(_highlight);
	add(_window, "window", "pauseMenu");
	add(_txtTitle, "text", "pauseMenu");
	add(_txtBody, "text", "pauseMenu");
	add(_btnNext, "button", "pauseMenu");
	add(_btnStop, "button", "pauseMenu");

	// Decide which half of the screen the panel occupies BEFORE
	// enableUiScaling captures the geometry — moving widgets afterwards would
	// require re-deriving State's private scale math. Placement is then fixed
	// for the popup's lifetime (Slice A: "keep the window fixed").
	placeForAnchor();

	centerAllSurfaces();

	enableUiScaling(320, 200, 0.75f);
	applyTTFToTexts(_game->getMod()->getTTFFont("FONT_HD_HUD", false), 0.92f);

	// Set up objects
	setWindowBackground(_window, "pauseMenu");

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_CAL_TUT_TITLE"));

	_txtBody->setWordWrap(true);

	_btnStop->setText(tr("STR_CAL_TUT_STOP"));
	_btnStop->onMouseClick((ActionHandler)&CalypsoTutorialState::btnStopClick);

	_btnNext->onMouseClick((ActionHandler)&CalypsoTutorialState::btnNextClick);
	// Esc skips the rest of the CURRENT step's pages, then advances.
	_btnNext->onKeyboardPress((ActionHandler)&CalypsoTutorialState::btnEscClick, Options::keyCancel);

	if (_game->getSavedGame() && _game->getSavedGame()->getSavedBattle())
	{
		applyBattlescapeTheme("pauseMenu");
	}
}

CalypsoTutorialState::~CalypsoTutorialState()
{
	// Let the manager push the next queued batch (if any). The State base
	// owns the widgets — do not delete them manually.
	CalypsoTutorial::get().notifyPopupClosed();
}

void CalypsoTutorialState::resize(int& dX, int& dY)
{
#ifdef __EMSCRIPTEN__
	applyUiScaling();
#else
	State::resize(dX, dY);
#endif
}

void CalypsoTutorialState::init()
{
	State::init();
	showPage();
}

void CalypsoTutorialState::think()
{
	State::think();
	_pulse++;
	drawHighlight();
}

/**
 * Places the panel in the half of the screen opposite the first page's anchor.
 * If the anchor's vertical centre is in the bottom half, flip the panel to the
 * top half; otherwise it stays at the construction default (bottom half).
 * Decided once, pre-scaling. Design-space Y values mirror the construction
 * offsets shifted to the top band.
 */
void CalypsoTutorialState::placeForAnchor()
{
	SDL_Rect r;
	bool haveAnchor = CalypsoTutorial::get().anchorRect(curAnchorKey(), r);
	bool anchorInBottomHalf = haveAnchor && (r.y + r.h / 2 >= 100);
	if (!anchorInBottomHalf) return; // keep bottom-half defaults

	_window->setY(16);
	_txtTitle->setY(22);
	_txtBody->setY(40);
	_btnNext->setY(68);
	_btnStop->setY(68);
}

/**
 * Renders the current page: body text, page counter in the title, and the
 * NEXT / GOT IT label (GOT IT only on the very last page of the batch).
 */
void CalypsoTutorialState::showPage()
{
	const CalypsoTutorialStep* s = cur();
	if (!s)
	{
		_game->popState();
		return;
	}

	_txtBody->setText(tr(s->pages[_pageIdx]));

	// "TUTORIAL (x/y)" — ASCII only (bitmap-font fallback cannot render more).
	std::ostringstream title;
	title << tr("STR_CAL_TUT_TITLE") << " (" << (_pageIdx + 1) << "/" << s->pages.size() << ")";
	_txtTitle->setText(title.str());

	bool lastPageOfLastStep = (_stepIdx + 1 >= _steps.size()) && (_pageIdx + 1 >= s->pages.size());
	_btnNext->setText(tr(lastPageOfLastStep ? "STR_CAL_TUT_GOTIT" : "STR_CAL_TUT_NEXT"));
}

/**
 * Advance one page; at the end of a step, mark it shown and move on; at the
 * end of the batch, pop the state.
 */
void CalypsoTutorialState::advance()
{
	const CalypsoTutorialStep* s = cur();
	if (s && _pageIdx + 1 < s->pages.size())
	{
		_pageIdx++;
		showPage();
		return;
	}
	// Last page of the current step — finish it and step forward.
	finishCurrentStep();
	_stepIdx++;
	_pageIdx = 0;
	if (_stepIdx >= _steps.size())
	{
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

void CalypsoTutorialState::btnNextClick(Action *) { advance(); }

void CalypsoTutorialState::btnEscClick(Action *)
{
	// Esc: abandon the rest of the current step, then advance as if finished.
	finishCurrentStep();
	advance();
}

void CalypsoTutorialState::btnStopClick(Action *)
{
	CalypsoTutorial::get().disableForCampaign();
	_game->popState();
}

const CalypsoTutorialStep* CalypsoTutorialState::cur() const
{
	return _stepIdx < _steps.size() ? _steps[_stepIdx] : nullptr;
}

std::string CalypsoTutorialState::curAnchorKey() const
{
	const CalypsoTutorialStep* s = cur();
	if (!s) return "";
	if (!s->pageAnchors.empty() && _pageIdx < s->pageAnchors.size() && !s->pageAnchors[_pageIdx].empty())
		return s->pageAnchors[_pageIdx];
	return s->anchor;
}

/**
 * Redraws the pulsing 2px border around the current page's anchor rect.
 * Clears the overlay to transparent first; if there is no anchor for this
 * page the popup simply shows without a border.
 *
 * Coordinate space: the highlight surface is resized by enableUiScaling to
 * fill the logical buffer, while anchors are registered in 320x200 design
 * space. We derive the live scale (surface / 320x200) and draw in surface
 * pixel space so the border lines up with the on-screen element. Border
 * thickness/gap are in screen pixels (constant regardless of scale).
 */
void CalypsoTutorialState::drawHighlight()
{
	_highlight->clear();

	SDL_Rect r;
	if (!CalypsoTutorial::get().anchorRect(curAnchorKey(), r)) return; // no anchor → no border

	// Pulse colour: alternate the pauseMenu window element colour with a +4
	// offset every 15 think() ticks. getElementOptional + INT_MAX guard keeps
	// this safe if the interface element is ever absent.
	RuleInterface* iface = _game->getMod()->getInterface("pauseMenu", false);
	const Element* el = iface ? iface->getElementOptional("window") : nullptr;
	Uint8 base = (el && el->color != INT_MAX) ? (Uint8)el->color : 13;
	Uint8 alt  = (Uint8)(base + 4);
	Uint8 color = (((_pulse / 15) % 2) == 0) ? base : alt;

	// Design→pixel scale (uniform; enableUiScaling preserves aspect).
	float sx = (float)_highlight->getWidth()  / 320.0f;
	float sy = (float)_highlight->getHeight() / 200.0f;
	float s = (sx < sy ? sx : sy);
	if (s <= 0.0f) s = 1.0f;

	// 2px screen-space frame around the anchor, inflated by 2px so it surrounds
	// (not covers) the element. Surface::drawRect clamps to the surface bounds,
	// so out-of-bounds edges (anchors near the screen edge) are safely clipped.
	const int pad = 2;    // screen px gap around the anchor
	const int thick = 2;  // screen px border thickness
	int ax = (int)(r.x * s) - pad;
	int ay = (int)(r.y * s) - pad;
	int aw = (int)(r.w * s) + 2 * pad;
	int ah = (int)(r.h * s) + 2 * pad;
	if (aw < 1) aw = 1;
	if (ah < 1) ah = 1;

	SDL_Rect top   { ax,           ay,            aw, thick };
	SDL_Rect bottom{ ax,           ay + ah - thick, aw, thick };
	SDL_Rect left  { ax,           ay,            thick, ah };
	SDL_Rect right { ax + aw - thick, ay,          thick, ah };

	_highlight->drawRect(&top, color);
	_highlight->drawRect(&bottom, color);
	_highlight->drawRect(&left, color);
	_highlight->drawRect(&right, color);
}

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
