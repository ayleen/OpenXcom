#ifdef __EMSCRIPTEN__

#include "CalypsoTextEdit.h"

#include <algorithm>
#include <cmath>

#include "CalypsoTextEditLayout.h"
#include "CalypsoTextInput.h"
#include "../Engine/Action.h"
#include "../Engine/Font.h"
#include "../Engine/Options.h"
#include "../Engine/State.h"
#include "../Engine/TTFFont.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextEdit.h"

extern "C" void calypso_notify_text_focus(int focused, int x, int y, int w, int h,
	const char *utf8, int multiline, int enterPolicy);

namespace OpenXcom
{
extern TextEdit *g_calypsoFocusedTextEdit;

namespace Calypso
{
namespace
{

int scaleMetric(int value, float fill)
{
	return std::max(0, static_cast<int>(value * fill + 0.5f));
}

CalypsoTextEditLayout layoutText(const UString& value, int width,
	Text* text, TTFFont* ttf, float fill)
{
	CalypsoTextEditMetrics metrics;
	if (ttf && ttf->measureGlyphs(value, metrics.advances, metrics.kerningBefore))
		return calypsoLayoutTextEdit(value, width, metrics, fill);
	metrics.advances.reserve(value.size());
	metrics.kerningBefore.assign(value.size(), 0);
	for (UCode c : value) metrics.advances.push_back(text->getFont()->getCharSize(c).w);
	return calypsoLayoutTextEdit(value, width, metrics);
}

int lineHeight(Text* text, TTFFont* ttf, float fill)
{
	if (ttf) return std::max(1, scaleMetric(ttf->lineHeight(), fill));
	return std::max(1, text->getFont()->getCharSize('\n').h);
}

int lineX(Text* text, int editorWidth, int width)
{
	switch (text->getAlign())
	{
	case ALIGN_CENTER: return (editorWidth - width) / 2;
	case ALIGN_RIGHT: return editorWidth - width;
	case ALIGN_LEFT: default: return 0;
	}
}

int blockY(Text* text, int editorHeight, int lineCount, int height,
	size_t firstVisibleLine)
{
	const int contentHeight = lineCount * height;
	if (contentHeight > editorHeight || firstVisibleLine != 0)
		return -static_cast<int>(firstVisibleLine) * height;
	switch (text->getVerticalAlign())
	{
	case ALIGN_MIDDLE: return (editorHeight - contentHeight) / 2;
	case ALIGN_BOTTOM: return editorHeight - contentHeight;
	case ALIGN_TOP: default: return 0;
	}
}

} // namespace

void CalypsoTextEdit::invalidateLayout(TextEdit& edit)
{
	edit._multilineLayoutValid = false;
}

void CalypsoTextEdit::ensureLayout(TextEdit& edit)
{
	if (edit._multilineLayoutValid || !edit._multiline || !edit._text->getFont()) return;
	edit._multilineDirectTTF = edit._ttf && edit.isARGB()
		&& edit.getEffectivePalette() && edit._ttf->lineHeight() > 0;
	edit._multilineMetricTTF = edit._multilineDirectTTF ? edit._ttf : nullptr;
	edit._multilineLayout = layoutText(edit._value, edit.getWidth(), edit._text,
		edit._multilineMetricTTF, edit._ttfFill);
	edit._multilineLineHeight = lineHeight(edit._text, edit._multilineMetricTTF, edit._ttfFill);
	edit._multilineLayoutValid = true;
}

void CalypsoTextEdit::updateViewport(TextEdit& edit)
{
	if (!edit._multiline || !edit._text->getFont()) return;
	ensureLayout(edit);
	if (!edit._multilineLayoutValid) return;
	const auto caret = calypsoLocateTextEditCaret(edit._multilineLayout, edit._caretPos);
	edit._firstVisibleLine = calypsoTextEditFirstVisibleLine(
		edit._multilineLayout.lines.size(), caret.line, edit._firstVisibleLine,
		std::max<size_t>(1, edit.getHeight() / edit._multilineLineHeight));
}

void CalypsoTextEdit::assignText(TextEdit& edit, const char* utf8)
{
	const UString incoming = Unicode::convUtf8ToUtf32(utf8 ? utf8 : "");
	edit._value = edit._multiline ? calypsoNormalizeTextEditNewlines(incoming) : incoming;
	edit._caretPos = edit._value.length();
	edit._firstVisibleLine = 0;
	edit._preferredCaretX = -1;
	invalidateLayout(edit);
	edit._redraw = true;
}

void CalypsoTextEdit::setMultiline(TextEdit& edit, bool multiline)
{
	if (edit._multiline == multiline) return;
	edit._multiline = multiline;
	if (edit._multiline)
	{
		edit._value = calypsoNormalizeTextEditNewlines(edit._value);
		edit._caretPos = std::min(edit._caretPos, edit._value.size());
		edit._text->setWidth(edit.getWidth());
		edit._text->setHeight(edit.getHeight());
		edit._text->setWordWrap(false);
	}
	edit._firstVisibleLine = 0;
	edit._preferredCaretX = -1;
	invalidateLayout(edit);
	edit._redraw = true;
}

void CalypsoTextEdit::resized(TextEdit& edit)
{
	if (edit._multiline)
	{
		edit._text->setWidth(edit.getWidth());
		edit._text->setHeight(edit.getHeight());
	}
	invalidateLayout(edit);
}

bool CalypsoTextEdit::hasEffectivePalette(const TextEdit& edit)
{
	return edit.getEffectivePalette() != nullptr;
}

void CalypsoTextEdit::paletteChanged(TextEdit& edit, bool hadEffectivePalette)
{
	if (hadEffectivePalette != (edit.getEffectivePalette() != nullptr)) invalidateLayout(edit);
}

void CalypsoTextEdit::textInput(TextEdit& edit, Action* action, State* state)
{
	if (Options::keyboardMode != KEYBOARD_ON
		|| !calypsoTextEventMayInsert(CalypsoTextEventSource::TextInput))
		return;
	const UString incoming = calypsoNormalizeTextInput(
		Unicode::convUtf8ToUtf32(action->getDetails()->text.text),
		edit._multiline && edit._textEditConstraint == TEC_NONE);
	// Enter policy applies to KEYDOWN. Hard breaks in SDL_TEXTINPUT are paste/IME
	// content; the DOM bridge prevents the commit Enter before it creates input.
	bool changed = false;
	for (UCode c : incoming)
	{
		if (c == '\n')
		{
			edit._value.insert(edit._caretPos++, 1, c);
			changed = true;
			continue;
		}
		if (!edit.isValidChar(c) || (!edit._multiline && edit.exceedsMaxWidth(c))) continue;
		edit._value.insert(edit._caretPos++, 1, c);
		changed = true;
	}
	if (!changed) return;
	edit._preferredCaretX = -1;
	if (edit._multiline)
	{
		invalidateLayout(edit);
		updateViewport(edit);
	}
	edit._redraw = true;
	if (edit._change) (state->*edit._change)(action);
}

bool CalypsoTextEdit::draw(TextEdit& edit)
{
	if (!edit._multiline) return false;

	SDL_Color rgba = {0, 0, 0, 255};
	const SDL_Color* palette = edit.getEffectivePalette();
	ensureLayout(edit);
	if (!edit._multilineLayoutValid) return true;
	const bool directTTF = edit._multilineDirectTTF;
	if (directTTF)
	{
		int idx = static_cast<int>(edit.getColor()) + 4 * (edit._highContrast ? 3 : 1);
		idx = std::max(0, std::min(255, idx));
		rgba = palette[idx];
		rgba.a = 255;
	}
	const auto& layout = edit._multilineLayout;
	const auto caret = calypsoLocateTextEditCaret(layout, edit._caretPos);
	const int height = edit._multilineLineHeight;
	const size_t visibleLines = std::max<size_t>(1, edit.getHeight() / height);
	edit._firstVisibleLine = calypsoTextEditFirstVisibleLine(
		layout.lines.size(), caret.line, edit._firstVisibleLine, visibleLines);
	const int yOffset = blockY(edit._text, edit.getHeight(),
		static_cast<int>(layout.lines.size()), height, edit._firstVisibleLine);

	edit.clear();
	if (edit._enter && edit._drawBackground)
	{
		SDL_Rect square = {0, 0, edit.getWidth(), edit.getHeight()};
		edit.drawRect(&square, edit.getColor());
	}
	if (directTTF)
	{
		for (size_t row = edit._firstVisibleLine; row < layout.lines.size(); ++row)
		{
			const int y = yOffset + static_cast<int>(row) * height;
			if (y >= edit.getHeight()) break;
			if (y + height <= 0) continue;
			const auto& line = layout.lines[row];
			const std::string utf8 = Unicode::convUtf32ToUtf8(
				edit._value.substr(line.start, line.end - line.start));
			if (utf8.empty()) continue;
			SDL_Surface* rendered = edit._ttf->renderText(utf8, rgba);
			if (!rendered) continue;
			SDL_SetSurfaceBlendMode(rendered, SDL_BLENDMODE_BLEND);
			SDL_Rect dst = {lineX(edit._text, edit.getWidth(), line.width()), y,
				std::max(1, scaleMetric(rendered->w, edit._ttfFill)),
				std::max(1, scaleMetric(rendered->h, edit._ttfFill))};
			SDL_BlitScaled(rendered, nullptr, edit.getSurface(), &dst);
		}
	}
	else
	{
		edit._text->setTTFFont(nullptr);
		UString display;
		const bool clipped = layout.lines.size() > visibleLines;
		const size_t first = clipped ? edit._firstVisibleLine : 0;
		const size_t last = clipped
			? std::min(layout.lines.size(), first + visibleLines) : layout.lines.size();
		for (size_t row = first; row < last; ++row)
		{
			const auto& line = layout.lines[row];
			display.append(edit._value, line.start, line.end - line.start);
			if (row + 1 < last) display.push_back('\n');
		}
		const TextVAlign oldAlign = edit._text->getVerticalAlign();
		if (clipped) edit._text->setVerticalAlign(ALIGN_TOP);
		edit._text->setText(Unicode::convUtf32ToUtf8(display));
		edit._text->blit(edit.getSurface());
		if (clipped) edit._text->setVerticalAlign(oldAlign);
		edit._text->setTTFFont(edit._ttf, edit._ttfFill);
	}

	if (Options::keyboardMode == KEYBOARD_ON && edit._isFocused && edit._blink)
	{
		const auto& line = layout.lines[caret.line];
		const int x = lineX(edit._text, edit.getWidth(), line.width()) + caret.x;
		const int y = yOffset + static_cast<int>(caret.line) * height;
		if (directTTF)
		{
			SDL_Rect caretRect = {x, y, std::max(1, scaleMetric(2, edit._ttfFill)), height};
			SDL_FillRect(edit.getSurface(), &caretRect, SDL_MapRGBA(edit.getSurface()->format,
				rgba.r, rgba.g, rgba.b, rgba.a));
		}
		else
		{
			edit._caret->setX(x);
			edit._caret->setY(y);
			edit._caret->blit(edit.getSurface());
		}
	}
	return true;
}

bool CalypsoTextEdit::mousePress(TextEdit& edit, Action* action, State* state)
{
	if (!edit._multiline) return false;
	ensureLayout(edit);
	if (!edit._multilineLayoutValid) return true;
	const auto& layout = edit._multilineLayout;
	const double sx = action->getXScale() > 0.0 ? action->getXScale() : 1.0;
	const double sy = action->getYScale() > 0.0 ? action->getYScale() : 1.0;
	const int height = edit._multilineLineHeight;
	const size_t visibleLines = std::max<size_t>(1, edit.getHeight() / height);
	const auto currentCaret = calypsoLocateTextEditCaret(layout, edit._caretPos);
	edit._firstVisibleLine = calypsoTextEditFirstVisibleLine(
		layout.lines.size(), currentCaret.line, edit._firstVisibleLine, visibleLines);
	const int yOffset = blockY(edit._text, edit.getHeight(),
		static_cast<int>(layout.lines.size()), height, edit._firstVisibleLine);
	const int line = std::max(0, std::min(static_cast<int>(layout.lines.size()) - 1,
		static_cast<int>(((action->getRelativeYMouse() / sy) - yOffset) / height)));
	const int xOffset = lineX(edit._text, edit.getWidth(), layout.lines[line].width());
	edit._caretPos = calypsoTextEditPositionAtX(layout.lines[line],
		static_cast<int>(action->getRelativeXMouse() / sx) - xOffset);
	edit._preferredCaretX = -1;
	updateViewport(edit);
	edit.InteractiveSurface::mousePress(action, state);
	return true;
}

bool CalypsoTextEdit::keyboardPress(TextEdit& edit, Action* action, State* state)
{
	if (!edit._multiline || Options::keyboardMode != KEYBOARD_ON) return false;
	ensureLayout(edit);
	if (!edit._multilineLayoutValid)
	{
		edit.InteractiveSurface::keyboardPress(action, state);
		return true;
	}
	bool changed = false;
	bool commitPressed = false;
	const SDL_Keycode key = action->getDetails()->key.keysym.sym;
	switch (key)
	{
	case SDLK_LEFT:
		if (edit._caretPos > 0) --edit._caretPos;
		edit._preferredCaretX = -1;
		break;
	case SDLK_RIGHT:
		if (edit._caretPos < edit._value.length()) ++edit._caretPos;
		edit._preferredCaretX = -1;
		break;
	case SDLK_HOME:
		edit._caretPos = edit._multilineLayout.lines[
			calypsoLocateTextEditCaret(edit._multilineLayout, edit._caretPos).line].start;
		edit._preferredCaretX = -1;
		break;
	case SDLK_END:
		edit._caretPos = edit._multilineLayout.lines[
			calypsoLocateTextEditCaret(edit._multilineLayout, edit._caretPos).line].end;
		edit._preferredCaretX = -1;
		break;
	case SDLK_UP:
	case SDLK_DOWN:
		{
			const auto move = calypsoMoveTextEditVertically(edit._multilineLayout,
				edit._caretPos, key == SDLK_UP ? -1 : 1, edit._preferredCaretX);
			edit._caretPos = move.position;
			edit._preferredCaretX = move.preferredX;
		}
		break;
	case SDLK_BACKSPACE:
		if (edit._caretPos > 0)
		{
			edit._value.erase(edit._caretPos - 1, 1);
			--edit._caretPos;
			changed = true;
		}
		edit._preferredCaretX = -1;
		break;
	case SDLK_DELETE:
		if (edit._caretPos < edit._value.length())
		{
			edit._value.erase(edit._caretPos, 1);
			changed = true;
		}
		edit._preferredCaretX = -1;
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (edit._enterPolicy == TEEP_COMMIT
			&& (action->getDetails()->key.keysym.mod & KMOD_SHIFT) == 0)
		{
			commitPressed = true;
		}
		else if (edit._textEditConstraint == TEC_NONE)
		{
			edit._value.insert(edit._caretPos++, 1, '\n');
			changed = true;
		}
		edit._preferredCaretX = -1;
		break;
	case SDLK_ESCAPE:
		break;
	default:
		break;
	}
	if (changed) invalidateLayout(edit);
	updateViewport(edit);
	edit._redraw = true;
	if (changed && edit._change) (state->*edit._change)(action);
	if (commitPressed)
	{
		edit.commit(action);
		return true;
	}
	edit.InteractiveSurface::keyboardPress(action, state);
	return true;
}

void CalypsoTextEdit::setTextExternal(TextEdit& edit, const char* utf8)
{
	const UString previousValue = edit._value;
	const std::u32string incoming = Unicode::convUtf8ToUtf32(utf8 ? utf8 : "");
	edit._value.clear();
	edit._caretPos = 0;
	if (!edit._multiline)
	{
		for (UCode c : incoming)
		{
			if (edit.isValidChar(c) && !edit.exceedsMaxWidth(c))
				edit._value.insert(edit._caretPos++, 1, c);
		}
	}
	else
	{
		for (UCode c : calypsoNormalizeTextEditNewlines(incoming))
		{
			if ((c == '\n' && edit._textEditConstraint == TEC_NONE) || edit.isValidChar(c))
				edit._value.insert(edit._caretPos++, 1, c);
		}
	}
	edit._firstVisibleLine = 0;
	edit._preferredCaretX = -1;
	invalidateLayout(edit);
	edit._redraw = true;
	if (edit._change && edit._state && edit._value != previousValue)
	{
		SDL_Event fakeEv;
		SDL_zero(fakeEv);
		fakeEv.type = SDL_KEYDOWN;
		Action fake(&fakeEv, 0.0, 0.0, 0, 0);
		(edit._state->*edit._change)(&fake);
	}
}

void CalypsoTextEdit::refreshExternalGeometry(TextEdit& edit)
{
	if (!edit._isFocused || g_calypsoFocusedTextEdit != &edit) return;
	calypso_notify_text_focus(2, edit.getX(), edit.getY(), edit.getWidth(), edit.getHeight(), "",
		edit._multiline ? 1 : 0, static_cast<int>(edit._enterPolicy));
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
