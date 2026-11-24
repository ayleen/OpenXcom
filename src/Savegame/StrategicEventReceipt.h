#pragma once
#include <map>
#include <string>

namespace OpenXcom
{
struct StrategicEventReceipt
{
	int order = 0;
	bool applied = false;
	bool acknowledged = false;
	std::string region, research, bonusResearch, recoveryFile;
	int city = -1;
	std::map<std::string, int> items;

	bool needsPresentation() const { return order > 0 && !acknowledged; }

	template<typename Apply>
	void applyOnce(Apply apply)
	{
		if (applied) return;
		apply();
		applied = true;
	}
};
}
