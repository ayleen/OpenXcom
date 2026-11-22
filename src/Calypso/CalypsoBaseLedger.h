#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsRenderer.h"
namespace OpenXcom
{
class BaseInfoState;
class MonthlyCostsState;
class StoresState;
class TransfersState;
class ItemLocationsState;
namespace Calypso
{
class CalypsoBaseLedger
{
public:
	explicit CalypsoBaseLedger(BaseInfoState &state);
	explicit CalypsoBaseLedger(MonthlyCostsState &state);
	explicit CalypsoBaseLedger(StoresState &state);
	explicit CalypsoBaseLedger(TransfersState &state);
	explicit CalypsoBaseLedger(ItemLocationsState &state);
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
