#ifdef __EMSCRIPTEN__
#pragma once
/*
 * Phase 41 (Calypso) -- transient radio-line toast popup.
 * Whole file is Emscripten-only -- the native desktop build never sees it.
 *
 * A minimal invisible modal (_screen=false) that shows one translated string
 * (tr(stringId)) as a brief narrative beat and auto-dismisses after a short
 * frame budget. It renders through the SAME DOM overlay the tutorial popup uses
 * (the global __calypsoTutorialShow/__calypsoTutorialHide hooks the web shell
 * already defines for CalypsoTutorialState), so this commit needs no web-shell
 * change; a later commit may skin a dedicated radio overlay if desired.
 *
 * Modeled on CalypsoTutorialState (the tutorial popup) but trimmed to a single
 * string owned BY VALUE -- CalypsoTutorialState cannot be reused here because
 * its step pointers are borrowed from the mod's step table and would dangle for
 * an ad-hoc single line, and its chrome ("Tutorial 1/1", Got-it/Disable) is
 * wrong for a radio beat. See CalypsoDirector::radioLine.
 */

#include <string>
#include "../Engine/State.h"

namespace OpenXcom
{

class CalypsoRadioLineState : public State
{
public:
	explicit CalypsoRadioLineState(std::string stringId);
	~CalypsoRadioLineState();
	void init() override;
	void think() override;
private:
	std::string _stringId;
	int _ticks = 0;
};

} // namespace OpenXcom

#endif // __EMSCRIPTEN__
