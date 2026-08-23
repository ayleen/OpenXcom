#include "../Calypso/CalypsoGeoscapeHdGlobeDirect.h"

void Globe::setGpuDirect(bool on)
{
	OpenXcom::CalypsoGeoscapeHdGlobeDirect::setGpuDirect(this, on);
}

namespace OpenXcom
{

void Globe::setGpuDirect(bool on)
{
	CalypsoGeoscapeHdGlobeDirect::setGpuDirect(this, on);
}

const double Globe::ROTATE_LONGITUDE = 0.10;
const double Globe::ROTATE_LATITUDE = 0.06;








































































