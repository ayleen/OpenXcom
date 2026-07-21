/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.2-HD (Calypso) -- see CalypsoHdWidgetBridge.h. Whole-file guard.
 */
#ifdef __EMSCRIPTEN__

#include "CalypsoHdWidgetBridge.h"

#include <unordered_set>

namespace OpenXcom
{
namespace Calypso
{

namespace
{
std::uint64_t s_frame = 0;
std::unordered_set<const void*> s_claimed;
} // namespace

void calypsoHdWidgetClaim(const void* widget, std::uint64_t frameId)
{
	if (frameId != s_frame)
	{
		s_claimed.clear();
		s_frame = frameId;
	}
	if (widget) s_claimed.insert(widget);
}

bool calypsoHdWidgetClaimed(const void* widget, std::uint64_t frameId)
{
	if (frameId != s_frame || !widget) return false;
	return s_claimed.find(widget) != s_claimed.end();
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
