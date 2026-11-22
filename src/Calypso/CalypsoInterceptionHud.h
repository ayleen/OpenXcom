#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdUiModel.h"
#include "CalypsoHdImageModel.h"
#include <vector>
#include <map>

namespace OpenXcom
{
class GeoscapeState;
class Action;
namespace Calypso
{
class CalypsoHdFrameBuilder;

struct CalypsoInterceptionView
{
	struct Control { CalypsoLogicalRect rect; int command, slot; };
	std::vector<Control> controls;
	std::map<int, std::pair<std::string, double>> labelWidths;
	CalypsoLogicalRect card{};
	CalypsoHdImageDescriptor ship, target;
	int focus = -1;
	bool confirming = false;
};

class CalypsoInterceptionHud
{
public:
	static void collect(GeoscapeState &state, CalypsoHdFrameBuilder &builder);
	static bool handle(GeoscapeState &state, Action *action);
};
}
}
#endif
