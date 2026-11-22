#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdOperationsChrome.h"
#include "../Mod/RuleBaseFacilityFunctions.h"
#include "CalypsoHdOperationsRenderer.h"
namespace OpenXcom
{
class GlobalAlienContainmentState;
class ManageAlienContainmentState;
class ContainmentReviewState;
class BaseInfoState;
class CalypsoEconomyState;
class CalypsoContractDeliverState;
class TechTreeSelectState;
class MonthlyCostsState;
class TargetInfoState;
class UfoTrackerState;
class MultipleTargetsState;
class StoresState;
class TransfersState;
class ItemsArrivingState;
class UfopaediaStartState;
class UfopaediaSelectState;
class ArticleState;
class StatsForNerdsState;
class GeoscapeEventState;
class ProductionCompleteState;
class Base;
class TextList;
class Text;
class TextButton;
class ItemLocationsState;
namespace Calypso
{
void calypsoAddUnlockRow(State &state, TextList &list, const std::string &label,
	RuleBaseFacilityFunctions required);

class CalypsoBaseLedger
{
public:
	explicit CalypsoBaseLedger(ManageAlienContainmentState &state);
	explicit CalypsoBaseLedger(ContainmentReviewState &state);
	explicit CalypsoBaseLedger(GlobalAlienContainmentState &state);
	explicit CalypsoBaseLedger(BaseInfoState &state);
	explicit CalypsoBaseLedger(CalypsoEconomyState &state);
	explicit CalypsoBaseLedger(CalypsoContractDeliverState &state);
	explicit CalypsoBaseLedger(TechTreeSelectState &state);
	explicit CalypsoBaseLedger(MonthlyCostsState &state);
	explicit CalypsoBaseLedger(StoresState &state);
	explicit CalypsoBaseLedger(UfoTrackerState &state);
	explicit CalypsoBaseLedger(TargetInfoState &state);
	explicit CalypsoBaseLedger(MultipleTargetsState &state);
	explicit CalypsoBaseLedger(TransfersState &state);
	explicit CalypsoBaseLedger(ItemLocationsState &state);
	explicit CalypsoBaseLedger(ItemsArrivingState &state);
	explicit CalypsoBaseLedger(UfopaediaStartState &state);
	explicit CalypsoBaseLedger(UfopaediaSelectState &state);
	explicit CalypsoBaseLedger(ArticleState &state);
	explicit CalypsoBaseLedger(StatsForNerdsState &state);
	explicit CalypsoBaseLedger(GeoscapeEventState &state);
	explicit CalypsoBaseLedger(ProductionCompleteState &state);
	CalypsoBaseLedger(State &state, Base *base, Text &title, TextList &possibilities,
		TextButton &openDestination, TextButton &ok);
	void refresh();
private:
	CalypsoHdOperationsChrome _chrome;
	CalypsoHdOperationsRenderer _renderer;
	std::function<CalypsoHdOperationsModel()> _collect;
	void open();
};
}
}
#endif
