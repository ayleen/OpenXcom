#pragma once
/* SDL adapter for the native-testable semantic focus coordinator. */
#include <SDL.h>

#include "../Engine/SDL2Helpers.h"
#include "CalypsoFocusCoordinator.h"

namespace OpenXcom
{
namespace Calypso
{

inline Uint8 calypsoFocusKeyBit(SDLKey key)
{
	switch (key)
	{
	case SDLK_TAB:      return 1u << 0;
	case SDLK_RETURN:   return 1u << 1;
	case SDLK_KP_ENTER: return 1u << 2;
	case SDLK_SPACE:    return 1u << 3;
	default:            return 0;
	}
}

/// Consumes semantic focus key pairs before State's ordinary surface dispatch.
/// Returns true when the caller must stop dispatching the mutated event.
inline bool calypsoHandleFocusEvent(SDL_Event* event,
	CalypsoFocusCoordinator& coordinator, Uint8& consumedKeys)
{
	if (!event) return false;
	if (event->type == SDL_KEYUP)
	{
		const Uint8 bit = calypsoFocusKeyBit(event->key.keysym.sym);
		if (bit && (consumedKeys & bit))
		{
			consumedKeys &= ~bit;
			event->type = SDL_NOEVENT;
			return true;
		}
		return false;
	}
	if (event->type != SDL_KEYDOWN) return false;

	CalypsoFocusKey key = CalypsoFocusKey::Other;
	switch (event->key.keysym.sym)
	{
	case SDLK_TAB:      key = CalypsoFocusKey::Tab; break;
	case SDLK_RETURN:   key = CalypsoFocusKey::Return; break;
	case SDLK_KP_ENTER: key = CalypsoFocusKey::KeypadEnter; break;
	case SDLK_SPACE:    key = CalypsoFocusKey::Space; break;
	default: break;
	}
	const SDL_Keymod mod = static_cast<SDL_Keymod>(event->key.keysym.mod);
	bool repeat = false;
#if SDL_VERSION_ATLEAST(2,0,0)
	repeat = event->key.repeat != 0;
#endif
	const CalypsoFocusKeyDecision decision = calypsoClassifyFocusKeyDown(
		key, (mod & KMOD_SHIFT) != 0, (mod & KMOD_CTRL) != 0,
		(mod & KMOD_ALT) != 0, (mod & KMOD_GUI) != 0, repeat);
	if (!decision.recognized) return false;

	consumedKeys |= calypsoFocusKeyBit(event->key.keysym.sym);
	if (decision.invokeCommand)
		(void)coordinator.command(decision.command, coordinator.generation(), true);
	event->type = SDL_NOEVENT;
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
