#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdOperationsRenderer.h"
namespace OpenXcom
{
class StoresState;
class TransfersState;
class ItemLocationsState;
namespace Calypso
{
class CalypsoInventoryLedger
{
public:
	explicit CalypsoInventoryLedger(StoresState &state);
	explicit CalypsoInventoryLedger(TransfersState &state);
	explicit CalypsoInventoryLedger(ItemLocationsState &state);
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
