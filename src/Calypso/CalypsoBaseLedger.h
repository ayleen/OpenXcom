#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsRenderer.h"
namespace OpenXcom
{
class GlobalAlienContainmentState;
class ManageAlienContainmentState;
class ContainmentReviewState;
class BaseInfoState;
class FundingState;
class MonthlyReportState;
class CalypsoEconomyState;
class CalypsoContractDeliverState;
class TechTreeSelectState;
class MonthlyCostsState;
class TargetInfoState;
class UfoTrackerState;
class MultipleTargetsState;
class StoresState;
class TransfersState;
class ItemLocationsState;
namespace Calypso
{
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
	explicit CalypsoBaseLedger(FundingState &state);
	explicit CalypsoBaseLedger(MonthlyReportState &state);
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
