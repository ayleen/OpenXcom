#pragma once
#include "../Basescape/PlaceFacilityState.h"

namespace OpenXcom
{
class Globe;

class CalypsoAccessLift : public PlaceFacilityState
{
public:
	bool usesStartingPlacement() const override { return true; }
	CalypsoAccessLift(Base *base, Globe *globe, bool first, std::vector<RuleBaseFacility *> lifts);
	void init() override;
	void choose(RuleBaseFacility *rule);
private:
	void openPicker(Action *);
	void placeLift(Action *);
	Globe *_globe;
	bool _first, _needChoice;
	std::vector<RuleBaseFacility *> _lifts;
};
}
