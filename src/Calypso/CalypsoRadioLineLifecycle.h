#pragma once

namespace OpenXcom
{
namespace Calypso
{

// Game::pushState() can make a freshly-created state the top state before the
// following iteration calls State::init().  A narrative radio line must not
// interpret its zero-initialized timing fields as an expired timeout in that
// intervening frame.
inline bool radioNarrativeShouldDismiss(bool initialized, unsigned nowMs,
	unsigned shownAtMs, unsigned durationMs)
{
	return initialized && nowMs - shownAtMs >= durationMs;
}

} // namespace Calypso
} // namespace OpenXcom
