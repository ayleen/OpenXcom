#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoTextEditLine
{
	size_t start = 0;
	size_t end = 0; // exclusive; a hard line-break is not part of either line
	std::vector<int> advances{0};

	int width() const { return advances.empty() ? 0 : advances.back(); }
};

struct CalypsoTextEditLayout
{
	std::vector<CalypsoTextEditLine> lines;
};

struct CalypsoTextEditMetrics
{
	std::vector<int> advances;      // one glyph advance per source codepoint
	std::vector<int> kerningBefore; // pair adjustment before that codepoint
};

struct CalypsoTextEditCaret
{
	size_t line = 0;
	int x = 0;
};

struct CalypsoTextEditVerticalMove
{
	size_t position = 0;
	int preferredX = 0;
};

inline bool calypsoTextEditHardBreak(char32_t c)
{
	return c == U'\n' || c == U'\r' || c == 2; // TOK_NL_SMALL
}

inline bool calypsoTextEditWrapSpace(char32_t c)
{
	return c == U' ' || c == U'\t';
}

inline std::u32string calypsoNormalizeTextEditNewlines(const std::u32string &source)
{
	std::u32string result;
	result.reserve(source.size());
	for (size_t i = 0; i < source.size(); ++i)
	{
		if (source[i] == U'\r')
		{
			result.push_back(U'\n');
			if (i + 1 < source.size() && source[i + 1] == U'\n') ++i;
		}
		else result.push_back(source[i]);
	}
	return result;
}

/** Word-wrap editable text without changing its source indices. */
inline CalypsoTextEditLayout calypsoLayoutTextEdit(
	const std::u32string &text, int maxWidth,
	const CalypsoTextEditMetrics &metrics, float scale = 1.0f)
{
	CalypsoTextEditLayout layout;
	if (scale <= 0.0f) scale = 1.0f;
	size_t start = 0;
	while (start <= text.size())
	{
		CalypsoTextEditLine line;
		line.start = start;
		size_t lastBreak = std::u32string::npos;
		bool finished = false;
		int rawWidth = 0;
		for (size_t i = start; i < text.size(); ++i)
		{
			if (calypsoTextEditHardBreak(text[i]))
			{
				line.end = i;
				layout.lines.push_back(line);
				start = i + 1;
				finished = true;
				break;
			}
			const int advance = i < metrics.advances.size() ? std::max(0, metrics.advances[i]) : 0;
			const int kerning = i > start && i < metrics.kerningBefore.size()
				? metrics.kerningBefore[i] : 0;
			rawWidth = std::max(0, rawWidth + advance + kerning);
			const int prefixWidth = std::max(0, static_cast<int>(rawWidth * scale + 0.5f));
			if (maxWidth > 0 && i > start && prefixWidth > maxWidth)
			{
				const size_t end = lastBreak != std::u32string::npos && lastBreak > start
					? lastBreak : i;
				line.end = end;
				line.advances.resize(end - start + 1);
				layout.lines.push_back(line);
				start = end;
				finished = true;
				break;
			}
			line.advances.push_back(prefixWidth);
			if (calypsoTextEditWrapSpace(text[i])) lastBreak = i + 1;
		}
		if (!finished)
		{
			line.end = text.size();
			layout.lines.push_back(line);
			break;
		}
	}
	return layout;
}

inline CalypsoTextEditCaret calypsoLocateTextEditCaret(
	const CalypsoTextEditLayout &layout, size_t position)
{
	if (layout.lines.empty()) return {};
	// At a soft-wrap boundary the later line owns the caret position.
	for (size_t i = layout.lines.size(); i-- > 0;)
	{
		const auto &line = layout.lines[i];
		if (position >= line.start && position <= line.end)
		{
			const size_t column = std::min(position - line.start, line.advances.size() - 1);
			return {i, line.advances[column]};
		}
	}
	const auto &last = layout.lines.back();
	return {layout.lines.size() - 1, last.width()};
}

inline size_t calypsoTextEditPositionAtX(const CalypsoTextEditLine &line, int x)
{
	if (line.advances.empty()) return line.start;
	if (x <= 0) return line.start;
	for (size_t column = 1; column < line.advances.size(); ++column)
	{
		const int midpoint = line.advances[column - 1]
		                   + (line.advances[column] - line.advances[column - 1]) / 2;
		if (x < midpoint) return line.start + column - 1;
	}
	return line.end;
}

inline CalypsoTextEditVerticalMove calypsoMoveTextEditVertically(
	const CalypsoTextEditLayout &layout, size_t position, int direction,
	int preferredX = -1)
{
	if (layout.lines.empty()) return {position, std::max(0, preferredX)};
	const auto caret = calypsoLocateTextEditCaret(layout, position);
	const int target = std::max(0, std::min(
		static_cast<int>(layout.lines.size()) - 1,
		static_cast<int>(caret.line) + direction));
	const int x = preferredX < 0 ? caret.x : preferredX;
	return {calypsoTextEditPositionAtX(layout.lines[target], x), x};
}

inline size_t calypsoTextEditFirstVisibleLine(size_t lineCount, size_t caretLine,
	size_t currentFirst, size_t visibleLines)
{
	if (lineCount == 0) return 0;
	visibleLines = std::max<size_t>(1, visibleLines);
	const size_t maxFirst = lineCount > visibleLines ? lineCount - visibleLines : 0;
	currentFirst = std::min(currentFirst, maxFirst);
	if (caretLine < currentFirst) return caretLine;
	if (caretLine >= currentFirst + visibleLines)
		return std::min(maxFirst, caretLine - visibleLines + 1);
	return currentFirst;
}

inline std::u32string calypsoTextEditDisplayText(
	const std::u32string &source, const CalypsoTextEditLayout &layout)
{
	std::u32string result;
	result.reserve(source.size() + layout.lines.size());
	for (size_t i = 0; i < layout.lines.size(); ++i)
	{
		const auto &line = layout.lines[i];
		result.append(source, line.start, line.end - line.start);
		if (i + 1 < layout.lines.size()) result.push_back(U'\n');
	}
	return result;
}

} // namespace Calypso
} // namespace OpenXcom
