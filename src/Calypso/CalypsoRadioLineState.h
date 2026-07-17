#ifdef __EMSCRIPTEN__
#pragma once
/*
 * Phase 41 (Calypso) -- transient radio-line toast popup.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 *
 * A minimal invisible modal (_screen=false) that shows one translated string
 * (tr(stringId)) as a brief narrative beat and auto-dismisses after a short
 * frame budget. It renders through its OWN DOM overlay (the global
 * __calypsoRadioShow/__calypsoRadioHide hooks defined by
 * web/public/radio-overlay.js) -- a dedicated, non-modal toast channel kept
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
 * Still pushed as a State (not fire-and-forget JS): the 2s pause it imposes
 * on the battle is intentional pacing for the narrative beat, not a leftover
 * of the old shared-overlay design -- keep it even though the overlay itself
 * is now non-blocking (pointer-events:none).
 */

#include <string>
#include "../Engine/State.h"

namespace OpenXcom
{

enum class CalypsoRadioLineKind { Narrative, Instruction };

class CalypsoRadioLineState : public State
{
public:
	explicit CalypsoRadioLineState(std::string stringId, CalypsoRadioLineKind kind);
	~CalypsoRadioLineState();
	void init() override;
	void think() override;
	void handle(Action *action) override;
	void dismiss();
private:
	std::string _stringId;
	CalypsoRadioLineKind _kind;
	Uint32 _shownAt = 0;
	unsigned _durationMs = 0;
};

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
