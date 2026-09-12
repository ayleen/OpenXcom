#pragma once
// Shared responsive classifier for the R&D operations family (re-review P1).
// The layout class comes from the canonical logical (CSS) viewport — never
// from Options::baseXResolution, the physical backing size, or DPR. Both the
// native adapters and the browser reference must answer with the same class
// for the same viewport.
#include <cstdint>

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoHdOperationsLayoutClass
{
	Unsupported,
	Compact,
	Wide,
};

/// Approved rd-hd-ac-visual-contract breakpoints: wide only at
/// W >= 1280 && H >= 720; compact down to W >= 740 && H >= 360; anything
/// smaller is an explicit HD size failure, not a shrunken canvas.
inline CalypsoHdOperationsLayoutClass classifyCalypsoHdOperationsLayout(
	int logicalWidth, int logicalHeight)
{
	if (logicalWidth >= 1280 && logicalHeight >= 720)
	{
		return CalypsoHdOperationsLayoutClass::Wide;
	}
	if (logicalWidth >= 740 && logicalHeight >= 360)
	{
		return CalypsoHdOperationsLayoutClass::Compact;
	}
	return CalypsoHdOperationsLayoutClass::Unsupported;
}

} // namespace Calypso
} // namespace OpenXcom
