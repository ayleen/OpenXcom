#ifdef __EMSCRIPTEN__
#pragma once
/*
 * Phase 41 (Calypso) -- explicit instructional radio popup.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 *
 * A minimal invisible modal (_screen=false) that shows one translated string
 * (tr(stringId)) until the player explicitly continues. It renders through
 * its OWN DOM overlay (the global
 * __calypsoRadioShow/__calypsoRadioHide hooks defined by
 * web/public/radio-overlay.js) -- a dedicated radio overlay channel kept
 * separate from the tutorial popup's __calypsoTutorialShow/Hide channel
 * (bug 4, Phase 41 QA round 1: a same-frame tutorial popup could clobber a
 * radio line when they shared one channel).
 *
 * Modeled on CalypsoTutorialState (the tutorial popup) but trimmed to a single
 * string owned BY VALUE -- CalypsoTutorialState cannot be reused here because
 * its step pointers are borrowed from the mod's step table and would dangle for
 * an ad-hoc single line, and its chrome ("Tutorial 1/1", Got-it/Disable) is
 * wrong for a radio beat. See CalypsoDirector::radioLine.
 *
 * Only instructional lines are pushed as a State because they intentionally
 * pause play. Passive narrative lines go through enqueueCalypsoNarrativeRadioLine
 * below and never become the top game state.
 */

#include <functional>
#include <string>
#include "../Engine/State.h"

namespace OpenXcom
{

enum class CalypsoRadioLineKind { Narrative, Instruction };

class CalypsoRadioLineState : public State
{
public:
	explicit CalypsoRadioLineState(std::string stringId,
		std::function<void()> onDismissed = {});
	~CalypsoRadioLineState();
	void init() override;
	void handle(Action *action) override;
	void dismiss();
private:
	std::string _stringId;
	std::function<void()> _onDismissed;
};

/// Queue a passive narrative toast in the browser without pushing a State.
/// The optional continuation runs only after the queued line has remained
/// visible for its reading-time duration and the DOM overlay has hidden it.
void enqueueCalypsoNarrativeRadioLine(const std::string &body,
	std::function<void()> onDismissed = {});

/// Drop queued narrative lines and their continuations when the owning
/// scripted scene is torn down or replaced.
void cancelCalypsoNarrativeRadioLines();

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
