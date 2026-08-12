#pragma once

namespace OpenXcom
{

struct CalypsoVoiceResponseOwnership
{
	bool playCustom = false;
	bool playStock = true;
};

/// A one-shot response has exactly one owner. The established OXCE response
/// remains the fallback unless custom audio has accepted the event.
inline CalypsoVoiceResponseOwnership calypsoVoiceResponseOwnership(
	bool customHandled)
{
	return {customHandled, !customHandled};
}

}
