#pragma once

#include <cstddef>
#include <string>

namespace OpenXcom
{
namespace Calypso
{

inline bool calypsoNotePreviewSpace(char32_t c)
{
	return c == U' ' || c == U'\t' || c == U'\n' || c == U'\r'
		|| c == U'\f' || c == U'\v' || c == 0x00a0 || c == 0x1680
		|| (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029
		|| c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
}

/// Fixed-row list preview: collapse every hard break/whitespace run and
/// ellipsize by Unicode scalar count so row geometry can never be expanded by
/// note contents.
inline std::u32string calypsoNotePreview(const std::u32string& source,
	                                     std::size_t maxScalars)
{
	std::u32string normalized;
	normalized.reserve(source.size());
	bool pendingSpace = false;
	for (char32_t c : source)
	{
		if (calypsoNotePreviewSpace(c))
		{
			pendingSpace = !normalized.empty();
			continue;
		}
		if (pendingSpace) normalized.push_back(U' ');
		pendingSpace = false;
		normalized.push_back(c);
	}
	if (maxScalars == 0) return {};
	if (normalized.size() <= maxScalars) return normalized;
	normalized.resize(maxScalars > 1 ? maxScalars - 1 : 0);
	while (!normalized.empty() && normalized.back() == U' ') normalized.pop_back();
	normalized.push_back(U'\u2026');
	return normalized;
}

} // namespace Calypso
} // namespace OpenXcom
