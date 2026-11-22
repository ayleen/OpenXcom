#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdOperationsRenderer.h"
#include "../Engine/InteractiveSurface.h"

namespace OpenXcom {
class Craft;
class TextButton;
class TextList;
namespace Calypso {

// The launch state retains ownership of every action. This view reads campaign
// facts and the boarding roster without running launch validation or generating a battle.
class CalypsoMissionReview
{
public:
	using Facts = std::vector<std::pair<std::string, std::string>>;
	CalypsoMissionReview(State &state, Craft *craft, const std::string &target, Facts facts,
		TextButton *begin, TextButton *cancel, TextButton *patrol, ActionHandler tabHandler, const char *style);
	void changePage(Action *action);
	void refresh();

private:
	CalypsoHdOperationsModel model();
	void populate();
	State &_state;
	Craft *_craft;
	std::string _targetName;
	Facts _facts;
	TextButton *_begin, *_cancel, *_patrol, *_brief, *_roster;
	TextList *_rows;
	bool _showRoster = false;
	CalypsoHdOperationsRenderer _renderer;
};
} }
#endif
