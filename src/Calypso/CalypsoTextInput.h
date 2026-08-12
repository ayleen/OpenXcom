#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoTextEventSource { KeyDown, TextInput };

struct CalypsoTextFocusTeardown
{
	bool stopTextInput;
	bool dismissBridge;
};

/// Destruction cannot use TextEdit::setFocus(false): that path reaches the
/// owning State while it may already be tearing down. Plan only the global
/// SDL/bridge work which is safe and still must be balanced.
inline CalypsoTextFocusTeardown calypsoPlanTextFocusTeardown(
	bool focused, bool ownsBridge)
{
	return {focused, ownsBridge};
}

/// SDL2 printable text is accepted only from SDL_TEXTINPUT. KEYDOWN owns
/// navigation/deletion/commit/cancel and never synthesizes a character from a
/// key symbol (which would turn Ctrl+V, F1, Insert, etc. into text).
inline bool calypsoTextEventMayInsert(CalypsoTextEventSource source)
{
	return source == CalypsoTextEventSource::TextInput;
}

inline bool calypsoIsUnicodeScalar(char32_t value)
{
	const std::uint32_t c = static_cast<std::uint32_t>(value);
	return c <= 0x10ffffu && !(c >= 0xd800u && c <= 0xdfffu);
}

/// Validate one SDL_TEXTINPUT payload and normalize desktop paste/IME hard
/// breaks. CRLF is one newline; lone CR is a newline. Single-line and
/// constrained editors drop hard breaks entirely.
inline std::u32string calypsoNormalizeTextInput(
	const std::u32string& source, bool allowNewlines)
{
	std::u32string result;
	result.reserve(source.size());
	for (std::size_t i = 0; i < source.size(); ++i)
	{
		char32_t c = source[i];
		if (!calypsoIsUnicodeScalar(c)) continue;
		if (c == U'\r' || c == U'\n')
		{
			if (c == U'\r' && i + 1 < source.size() && source[i + 1] == U'\n') ++i;
			if (allowNewlines) result.push_back(U'\n');
			continue;
		}
		result.push_back(c);
	}
	return result;
}

} // namespace Calypso
} // namespace OpenXcom
