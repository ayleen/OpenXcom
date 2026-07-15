/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "TextEdit.h"
#include <cmath>
#include "../Engine/Action.h"
#include "../Engine/Font.h"
#include "../Engine/TTFFont.h"
#include "../Engine/Timer.h"
#include "../Engine/Options.h"
#include "../Calypso/CalypsoTextEditLayout.h"
#include "../Calypso/CalypsoTextInput.h"
#include "../fallthrough.h"

#ifdef __EMSCRIPTEN__
// Phase 33 (mobile): virtual-keyboard bridge. setFocus notifies JS via this
// harness hook (defined in Calypso/EmscriptenHarness.cpp, C linkage), and the
// harness writes back through g_calypsoFocusedTextEdit (C++ linkage, defined
// here — the harness references it with a matching namespaced extern).
extern "C" void calypso_notify_text_focus(int focused, int x, int y, int w, int h,
	const char *utf8, int multiline, int enterPolicy);
namespace OpenXcom { TextEdit *g_calypsoFocusedTextEdit = nullptr; }
#endif

namespace OpenXcom
{

namespace
{

int textEditScaleMetric(int value, float fill)
{
	return std::max(0, static_cast<int>(value * fill + 0.5f));
}

Calypso::CalypsoTextEditLayout textEditLayout(const UString &value, int width,
	Text *text, TTFFont *ttf, float fill)
{
	Calypso::CalypsoTextEditMetrics metrics;
	if (ttf && ttf->measureGlyphs(value, metrics.advances, metrics.kerningBefore))
		return Calypso::calypsoLayoutTextEdit(value, width, metrics, fill);
	metrics.advances.reserve(value.size());
	metrics.kerningBefore.assign(value.size(), 0);
	for (UCode c : value) metrics.advances.push_back(text->getFont()->getCharSize(c).w);
	return Calypso::calypsoLayoutTextEdit(value, width, metrics);
}

int textEditLineHeight(Text *text, TTFFont *ttf, float fill)
{
	if (ttf) return std::max(1, textEditScaleMetric(ttf->lineHeight(), fill));
	return std::max(1, text->getFont()->getCharSize('\n').h);
}

int textEditLineX(Text *text, int editorWidth, int lineWidth)
{
	switch (text->getAlign())
	{
	case ALIGN_CENTER: return (editorWidth - lineWidth) / 2;
	case ALIGN_RIGHT: return editorWidth - lineWidth;
	case ALIGN_LEFT: default: return 0;
	}
}

int textEditBlockY(Text *text, int editorHeight, int lineCount, int lineHeight,
	size_t firstVisibleLine)
{
	const int contentHeight = lineCount * lineHeight;
	if (contentHeight > editorHeight || firstVisibleLine != 0)
		return -static_cast<int>(firstVisibleLine) * lineHeight;
	switch (text->getVerticalAlign())
	{
	case ALIGN_MIDDLE: return (editorHeight - contentHeight) / 2;
	case ALIGN_BOTTOM: return editorHeight - contentHeight;
	case ALIGN_TOP: default: return 0;
	}
}

} // namespace

/**
 * Sets up a blank text edit with the specified size and position.
 * @param state Pointer to state the text edit belongs to.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
TextEdit::TextEdit(State *state, int width, int height, int x, int y) : InteractiveSurface(width, height, x, y),
	_blink(true), _modal(true), _drawBackground(true),
	_multiline(false), _highContrast(false), _ttf(nullptr), _ttfFill(1.0f),
	_char('A'), _caretPos(0), _firstVisibleLine(0), _preferredCaretX(-1),
	_enterPolicy(TEEP_INSERT_NEWLINE), _textEditConstraint(TEC_NONE),
	_multilineLayoutValid(false), _multilineDirectTTF(false),
	_multilineMetricTTF(nullptr), _multilineLineHeight(1),
	_change(0), _enter(0), _state(state)
{
	_isFocused = false;
	_text = new Text(width, height, 0, 0);
	_timer = new Timer(100);
	_timer->onTimer((SurfaceHandler)&TextEdit::blink);
	_caret = new Text(16, 17, 0, 0);
	_caret->setText("|");
}

/**
 * Deletes contents.
 */
TextEdit::~TextEdit()
{
	const Calypso::CalypsoTextFocusTeardown teardown =
		Calypso::calypsoPlanTextFocusTeardown(_isFocused,
#ifdef __EMSCRIPTEN__
			g_calypsoFocusedTextEdit == this);
#else
			false);
#endif
	if (teardown.stopTextInput)
	{
		// Do not call setFocus(false) here: State may be part-way through its
		// reverse surface destruction, so modal/focus callbacks are unsafe.
		InteractiveSurface::setFocus(false);
		_blink = false;
		_timer->stop();
		SDL_StopTextInput();
		SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
	}
#ifdef __EMSCRIPTEN__
	if (teardown.dismissBridge)
	{
		g_calypsoFocusedTextEdit = nullptr;
		calypso_notify_text_focus(0, 0, 0, 0, 0, "", 0, 0);
	}
#endif
	delete _text;
	delete _caret;
	delete _timer;
}

/**
 * Passes events to internal components.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextEdit::handle(Action *action, State *state)
{
	InteractiveSurface::handle(action, state);
	if (_isFocused && action->getDetails()->type == SDL_TEXTINPUT)
	{
		textInput(action, state);
		return;
	}
	if (_isFocused && _modal && action->getDetails()->type == SDL_MOUSEBUTTONDOWN &&
		(action->getAbsoluteXMouse() < getX() || action->getAbsoluteXMouse() >= getX() + getWidth() ||
		 action->getAbsoluteYMouse() < getY() || action->getAbsoluteYMouse() >= getY() + getHeight()))
	{
		setFocus(false);
	}
}

/**
 * Controls the blinking animation when
 * the text edit is focused.
 * @param focus True if focused, false otherwise.
 * @param modal True to lock input to this control, false otherwise.
 */
void TextEdit::setFocus(bool focus, bool modal)
{
	_modal = modal;
	if (focus != _isFocused)
	{
		_redraw = true;
		InteractiveSurface::setFocus(focus);
		if (_isFocused)
		{
			SDL_StartTextInput();
			SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
			_caretPos = _value.length();
			_blink = true;
			_timer->start();
			if (_modal)
				_state->setModal(this);
#ifdef __EMSCRIPTEN__
			g_calypsoFocusedTextEdit = this;
			calypso_notify_text_focus(1, getX(), getY(), getWidth(), getHeight(),
				Unicode::convUtf32ToUtf8(_value).c_str(), _multiline ? 1 : 0,
				static_cast<int>(_enterPolicy));
#endif
		}
		else
		{
			SDL_StopTextInput();
			_blink = false;
			_timer->stop();
			SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
			if (_modal)
				_state->setModal(0);
#ifdef __EMSCRIPTEN__
			if (g_calypsoFocusedTextEdit == this) g_calypsoFocusedTextEdit = nullptr;
			calypso_notify_text_focus(0, 0, 0, 0, 0, "", 0, 0);
#endif
		}
	}
}

void TextEdit::textInput(Action *action, State *state)
{
	if (Options::keyboardMode != KEYBOARD_ON
		|| !Calypso::calypsoTextEventMayInsert(Calypso::CalypsoTextEventSource::TextInput))
		return;
	const UString incoming = Calypso::calypsoNormalizeTextInput(
		Unicode::convUtf8ToUtf32(action->getDetails()->text.text),
		_multiline && _textEditConstraint == TEC_NONE);
	bool changed = false;
	for (UCode c : incoming)
	{
		if (c == '\n')
		{
			_value.insert(_caretPos++, 1, c);
			changed = true;
			continue;
		}
		if (!isValidChar(c) || (!_multiline && exceedsMaxWidth(c))) continue;
		_value.insert(_caretPos++, 1, c);
		changed = true;
	}
	if (!changed) return;
	_preferredCaretX = -1;
	if (_multiline)
	{
		invalidateMultilineLayout();
		updateMultilineViewport();
	}
	_redraw = true;
	if (_change) (state->*_change)(action);
}

/**
 * Changes the text edit to use the big-size font.
 */
void TextEdit::setBig()
{
	_text->setBig();
	_caret->setBig();
	invalidateMultilineLayout();
}

/**
 * Changes the text edit to use the small-size font.
 */
void TextEdit::setSmall()
{
	_text->setSmall();
	_caret->setSmall();
	invalidateMultilineLayout();
}

void TextEdit::setWidth(int width)
{
	Surface::setWidth(width);
	if (_multiline) _text->setWidth(width);
	invalidateMultilineLayout();
}

void TextEdit::setHeight(int height)
{
	Surface::setHeight(height);
	if (_multiline) _text->setHeight(height);
	invalidateMultilineLayout();
}

/**
 * Changes the various fonts for the text edit to use.
 * The different fonts need to be passed in advance since the
 * text size can change mid-text.
 * @param big Pointer to large-size font.
 * @param small Pointer to small-size font.
 * @param lang Pointer to current language.
 */
void TextEdit::initText(Font *big, Font *small, Language *lang)
{
	_text->initText(big, small, lang);
	_caret->initText(big, small, lang);
	invalidateMultilineLayout();
}

/**
 * Changes the string displayed on screen.
 * @param text Text string.
 */
void TextEdit::setText(const std::string &text)
{
	const UString incoming = Unicode::convUtf8ToUtf32(text);
	_value = _multiline ? Calypso::calypsoNormalizeTextEditNewlines(incoming) : incoming;
	_caretPos = _value.length();
	_firstVisibleLine = 0;
	_preferredCaretX = -1;
	invalidateMultilineLayout();
	_redraw = true;
}

#ifdef __EMSCRIPTEN__
/**
 * Phase 33 (Emscripten): replaces the whole value from the JS input overlay.
 * Mirrors the keydown insertion path: per-character isValidChar (numeric
 * constraints) + exceedsMaxWidth clamp, then fires the state's onChange
 * handler exactly like keyboardPress does — several states (base/soldier
 * rename) write the model from onChange, not from the final value, so an
 * engine-initiated unfocus must not lose the edit.
 * @param utf8 New value as a UTF-8 string.
 */
void TextEdit::setTextExternal(const std::string &utf8)
{
	const UString previousValue = _value;
	const std::u32string incoming = Unicode::convUtf8ToUtf32(utf8);
	_value.clear();
	_caretPos = 0;
	if (!_multiline)
	{
		for (UCode c : incoming)
		{
			if (isValidChar(c) && !exceedsMaxWidth(c))
			{
				_value.insert(_caretPos, 1, c);
				_caretPos++;
			}
		}
	}
	else
	{
		for (UCode c : Calypso::calypsoNormalizeTextEditNewlines(incoming))
		{
			if ((c == '\n' && _textEditConstraint == TEC_NONE) || isValidChar(c))
			{
				_value.insert(_caretPos++, 1, c);
			}
		}
	}
	_firstVisibleLine = 0;
	_preferredCaretX = -1;
	invalidateMultilineLayout();
	_redraw = true;
	if (_change && _state && _value != previousValue)
	{
		/* Zeroed KEYDOWN (sym = SDLK_UNKNOWN): handlers that special-case
		 * Enter/Escape take their normal-typing branch. */
		SDL_Event fakeEv;
		SDL_zero(fakeEv);
		fakeEv.type = SDL_KEYDOWN;
		Action fake(&fakeEv, 0.0, 0.0, 0, 0);
		(_state->*_change)(&fake);
	}
}

void TextEdit::refreshExternalGeometry()
{
	if (!_isFocused || g_calypsoFocusedTextEdit != this) return;
	calypso_notify_text_focus(2, getX(), getY(), getWidth(), getHeight(), "",
		_multiline ? 1 : 0, static_cast<int>(_enterPolicy));
}
#endif

/**
 * Returns the string displayed on screen.
 * @return Text string.
 */
std::string TextEdit::getText() const
{
	return Unicode::convUtf32ToUtf8(_value);
}

/**
 * Returns the pixel width of the current text (bitmap font), used to size the
 * edit's clickable hit-area to its content.
 * @return Text width in pixels.
 */
int TextEdit::getTextWidth() const
{
	return _text ? _text->getTextWidth() : 0;
}

/**
 * Enables/disables text wordwrapping. When enabled, lines of
 * text are automatically split to ensure they stay within the
 * drawing area, otherwise they simply go off the edge.
 * @param wrap Wordwrapping setting.
 */
void TextEdit::setWordWrap(bool wrap)
{
	_text->setWordWrap(wrap);
}

void TextEdit::setMultiline(bool multiline)
{
	if (_multiline == multiline) return;
	_multiline = multiline;
	if (_multiline)
	{
		_value = Calypso::calypsoNormalizeTextEditNewlines(_value);
		_caretPos = std::min(_caretPos, _value.size());
		_text->setWidth(getWidth());
		_text->setHeight(getHeight());
	}
	_firstVisibleLine = 0;
	_preferredCaretX = -1;
	if (_multiline) _text->setWordWrap(false);
	invalidateMultilineLayout();
	_redraw = true;
}

void TextEdit::setTTFFont(TTFFont *font, float fillFrac)
{
	_ttf = font;
	_ttfFill = fillFrac > 0.0f ? std::min(1.0f, fillFrac) : 1.0f;
	_text->setTTFFont(font, fillFrac);
	invalidateMultilineLayout();
}

/**
 * Enables/disables color inverting. Mostly used to make
 * button text look pressed along with the button.
 * @param invert Invert setting.
 */
void TextEdit::setInvert(bool invert)
{
	_text->setInvert(invert);
	_caret->setInvert(invert);
}

/**
 * Enables/disables high contrast color. Mostly used for
 * Battlescape text.
 * @param contrast High contrast setting.
 */
void TextEdit::setHighContrast(bool contrast)
{
	_highContrast = contrast;
	_text->setHighContrast(contrast);
	_caret->setHighContrast(contrast);
}

/**
 * Changes the way the text is aligned horizontally
 * relative to the drawing area.
 * @param align Horizontal alignment.
 */
void TextEdit::setAlign(TextHAlign align)
{
	_text->setAlign(align);
}

/**
 * Changes the way the text is aligned vertically
 * relative to the drawing area.
 * @param valign Vertical alignment.
 */
void TextEdit::setVerticalAlign(TextVAlign valign)
{
	_text->setVerticalAlign(valign);
}

/**
 * Restricts the text to only numerical input or signed numerical input.
 * @param constraint TextEditConstraint to be applied.
 */
void TextEdit::setConstraint(TextEditConstraint constraint)
{
	_textEditConstraint = constraint;
}

/**
 * Changes the color used to render the text. Unlike regular graphics,
 * fonts are greyscale so they need to be assigned a specific position
 * in the palette to be displayed.
 * @param color Color value.
 */
void TextEdit::setColor(Uint8 color)
{
	_text->setColor(color);
	_caret->setColor(color);
}

/**
 * Returns the color used to render the text.
 * @return Color value.
 */
Uint8 TextEdit::getColor() const
{
	return _text->getColor();
}

/**
 * Changes the secondary color used to render the text. The text
 * switches between the primary and secondary color whenever there's
 * a 0x01 in the string.
 * @param color Color value.
 */
void TextEdit::setSecondaryColor(Uint8 color)
{
	_text->setSecondaryColor(color);
}

/**
 * Returns the secondary color used to render the text.
 * @return Color value.
 */
Uint8 TextEdit::getSecondaryColor() const
{
	return _text->getSecondaryColor();
}

/**
 * Replaces a certain amount of colors in the text edit's palette.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 */
void TextEdit::setPalette(const SDL_Color *colors, int firstcolor, int ncolors)
{
	const bool hadEffectivePalette = getEffectivePalette() != nullptr;
	Surface::setPalette(colors, firstcolor, ncolors);
	_text->setPalette(colors, firstcolor, ncolors);
	_caret->setPalette(colors, firstcolor, ncolors);
	// Palette colors do not affect metrics; only first-time availability can switch
	// the cached renderer from bitmap fallback to authoritative direct TTF.
	if (hadEffectivePalette != (getEffectivePalette() != nullptr)) invalidateMultilineLayout();
}

/**
 * Keeps the animation timers running.
 */
void TextEdit::think()
{
	_timer->think(0, this);
}

/**
 * Plays the blinking animation when the
 * text edit is focused.
 */
void TextEdit::blink()
{
	_blink = !_blink;
	_redraw = true;
}

void TextEdit::invalidateMultilineLayout()
{
	_multilineLayoutValid = false;
}

void TextEdit::ensureMultilineLayout()
{
	if (_multilineLayoutValid || !_multiline || !_text->getFont()) return;
	_multilineDirectTTF = _ttf && isARGB() && getEffectivePalette() && _ttf->lineHeight() > 0;
	_multilineMetricTTF = _multilineDirectTTF ? _ttf : nullptr;
	_multilineLayout = textEditLayout(_value, getWidth(), _text, _multilineMetricTTF, _ttfFill);
	_multilineLineHeight = textEditLineHeight(_text, _multilineMetricTTF, _ttfFill);
	_multilineLayoutValid = true;
}

void TextEdit::updateMultilineViewport()
{
	if (!_multiline || !_text->getFont()) return;
	ensureMultilineLayout();
	if (!_multilineLayoutValid) return;
	const auto caret = Calypso::calypsoLocateTextEditCaret(_multilineLayout, _caretPos);
	_firstVisibleLine = Calypso::calypsoTextEditFirstVisibleLine(
		_multilineLayout.lines.size(), caret.line, _firstVisibleLine,
		std::max<size_t>(1, getHeight() / _multilineLineHeight));
}

/**
 * Adds a flashing | caret to the text
 * to show when it's focused and editable.
 */
void TextEdit::draw()
{
	Surface::draw();
	if (!_multiline)
	{
		UString newValue = _value;
		if (Options::keyboardMode == KEYBOARD_OFF && _isFocused && _blink) newValue += _char;
		_text->setText(Unicode::convUtf32ToUtf8(newValue));
		clear();
		if (_enter && _drawBackground)
		{
			SDL_Rect square = {0, 0, getWidth(), getHeight()};
			drawRect(&square, getColor());
		}
		_text->blit(this->getSurface());
		if (Options::keyboardMode == KEYBOARD_ON && _isFocused && _blink)
		{
			int x = 0;
			switch (_text->getAlign())
			{
			case ALIGN_LEFT: break;
			case ALIGN_CENTER: x = (_text->getWidth() - _text->getTextWidth()) / 2; break;
			case ALIGN_RIGHT: x = _text->getWidth() - _text->getTextWidth(); break;
			}
			for (size_t i = 0; i < _caretPos; ++i) x += _text->getFont()->getCharSize(_value[i]).w;
			_caret->setX(x);
			int y = 0;
			switch (_text->getVerticalAlign())
			{
			case ALIGN_TOP: y = 0; break;
			case ALIGN_MIDDLE: y = (int)ceil((getHeight() - _text->getTextHeight()) / 2.0); break;
			case ALIGN_BOTTOM: y = getHeight() - _text->getTextHeight(); break;
			}
			_caret->setY(y);
			_caret->blit(this->getSurface());
		}
		return;
	}

	SDL_Color rgba = {0, 0, 0, 255};
	const SDL_Color *palette = getEffectivePalette();
	ensureMultilineLayout();
	if (!_multilineLayoutValid) return;
	const bool directTTF = _multilineDirectTTF;
	if (directTTF)
	{
		int idx = static_cast<int>(getColor()) + 4 * (_highContrast ? 3 : 1);
		idx = std::max(0, std::min(255, idx));
		rgba = palette[idx];
		rgba.a = 255;
	}
	const auto &layout = _multilineLayout;
	const auto caret = Calypso::calypsoLocateTextEditCaret(layout, _caretPos);
	const int lineHeight = _multilineLineHeight;
	const size_t visibleLines = std::max<size_t>(1, getHeight() / lineHeight);
	_firstVisibleLine = Calypso::calypsoTextEditFirstVisibleLine(
		layout.lines.size(), caret.line, _firstVisibleLine, visibleLines);
	const int blockY = textEditBlockY(_text, getHeight(), static_cast<int>(layout.lines.size()),
		lineHeight, _firstVisibleLine);

	clear();
	if (_enter && _drawBackground)
	{
		SDL_Rect square = {0, 0, getWidth(), getHeight()};
		drawRect(&square, getColor());
	}
	if (directTTF)
	{
		for (size_t row = _firstVisibleLine; row < layout.lines.size(); ++row)
		{
			const int y = blockY + static_cast<int>(row) * lineHeight;
			if (y >= getHeight()) break;
			if (y + lineHeight <= 0) continue;
			const auto &line = layout.lines[row];
			const std::string utf8 = Unicode::convUtf32ToUtf8(_value.substr(line.start, line.end - line.start));
			if (utf8.empty()) continue;
			SDL_Surface *rendered = _ttf->renderText(utf8, rgba);
			if (!rendered) continue;
			SDL_SetSurfaceBlendMode(rendered, SDL_BLENDMODE_BLEND);
			SDL_Rect dst = {textEditLineX(_text, getWidth(), line.width()), y,
				std::max(1, textEditScaleMetric(rendered->w, _ttfFill)),
				std::max(1, textEditScaleMetric(rendered->h, _ttfFill))};
			SDL_BlitScaled(rendered, nullptr, getSurface(), &dst);
		}
	}
	else
	{
		_text->setTTFFont(nullptr);
		UString display;
		const bool clipped = layout.lines.size() > visibleLines;
		const size_t first = clipped ? _firstVisibleLine : 0;
		const size_t last = clipped ? std::min(layout.lines.size(), first + visibleLines) : layout.lines.size();
		for (size_t row = first; row < last; ++row)
		{
			const auto &line = layout.lines[row];
			display.append(_value, line.start, line.end - line.start);
			if (row + 1 < last) display.push_back('\n');
		}
		const TextVAlign oldAlign = _text->getVerticalAlign();
		if (clipped) _text->setVerticalAlign(ALIGN_TOP);
		_text->setText(Unicode::convUtf32ToUtf8(display));
		_text->blit(getSurface());
		if (clipped) _text->setVerticalAlign(oldAlign);
		_text->setTTFFont(_ttf, _ttfFill);
	}

	if (Options::keyboardMode == KEYBOARD_ON && _isFocused && _blink)
	{
		const auto &line = layout.lines[caret.line];
		const int x = textEditLineX(_text, getWidth(), line.width()) + caret.x;
		const int y = blockY + static_cast<int>(caret.line) * lineHeight;
		if (directTTF)
		{
			SDL_Rect caretRect = {x, y, std::max(1, textEditScaleMetric(2, _ttfFill)), lineHeight};
			SDL_FillRect(getSurface(), &caretRect, SDL_MapRGBA(getSurface()->format, rgba.r, rgba.g, rgba.b, rgba.a));
		}
		else
		{
			_caret->setX(x);
			_caret->setY(y);
			_caret->blit(getSurface());
		}
	}
}

/**
 * Checks if adding a certain character to
 * the text edit will exceed the maximum width.
 * Used to make sure user input stays within bounds.
 * @param c Character to add.
 * @return True if it exceeds, False if it doesn't.
 */
bool TextEdit::exceedsMaxWidth(UCode c) const
{
	int w = 0;
	UString s = _value;

	s += c;
	for (UString::const_iterator i = s.begin(); i < s.end(); ++i)
	{
		w += _text->getFont()->getCharSize(*i).w;
	}

	return (w > getWidth());
}

/**
 * Checks if input key character is valid to
 * be inserted at caret position in the text edit
 * without breaking the text edit constraint.
 * @param c Character to validate.
 * @return True if character can be inserted, False if it cannot.
 */
bool TextEdit::isValidChar(UCode c) const
{
	if (!Calypso::calypsoIsUnicodeScalar(c)) return false;
	switch (_textEditConstraint)
	{
	case TEC_NUMERIC_POSITIVE:
		return c >= '0' && c <= '9';

	// If constraint is "(signed) numeric", need to check:
	// - user does not input a character before '-' or '+'
	// - user enter either figure anywhere, or a sign at first position
	case TEC_NUMERIC:
		if (_caretPos > 0)
		{
			return c >= '0' && c <= '9';
		}
		else
		{
			return ((c >= '0' && c <= '9') || c == '+' || c == '-') &&
					(_value.empty() || (_value[0] != '+' && _value[0] != '-'));
		}

	case TEC_NONE:
		return (c >= ' ' && c <= '~') || c >= 160;

	default:
		return false;
	}
}

/**
 * Focuses the text edit when it's pressed on.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextEdit::mousePress(Action *action, State *state)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		if (!_isFocused)
		{
			setFocus(true);
		}
		else
		{
			if (_multiline)
			{
				ensureMultilineLayout();
				if (!_multilineLayoutValid) return;
				const auto &layout = _multilineLayout;
				const double sx = action->getXScale() > 0.0 ? action->getXScale() : 1.0;
				const double sy = action->getYScale() > 0.0 ? action->getYScale() : 1.0;
				const int lineHeight = _multilineLineHeight;
				const size_t visibleLines = std::max<size_t>(1, getHeight() / lineHeight);
				const auto currentCaret = Calypso::calypsoLocateTextEditCaret(layout, _caretPos);
				_firstVisibleLine = Calypso::calypsoTextEditFirstVisibleLine(
					layout.lines.size(), currentCaret.line, _firstVisibleLine, visibleLines);
				const int blockY = textEditBlockY(_text, getHeight(), static_cast<int>(layout.lines.size()),
					lineHeight, _firstVisibleLine);
				const int line = std::max(0, std::min(static_cast<int>(layout.lines.size()) - 1,
					static_cast<int>(((action->getRelativeYMouse() / sy) - blockY) / lineHeight)));
				const int lineX = textEditLineX(_text, getWidth(), layout.lines[line].width());
				_caretPos = Calypso::calypsoTextEditPositionAtX(
					layout.lines[line], static_cast<int>(action->getRelativeXMouse() / sx) - lineX);
				_preferredCaretX = -1;
				updateMultilineViewport();
				InteractiveSurface::mousePress(action, state);
				return;
			}
			double mouseX = action->getRelativeXMouse();
			double scaleX = action->getXScale();
			double w = 0;
			int c = 0;
			for (UString::iterator i = _value.begin(); i < _value.end(); ++i)
			{
				if (mouseX <= w)
				{
					break;
				}
				w += (double)_text->getFont()->getCharSize(*i).w / 2 * scaleX;
				if (mouseX <= w)
				{
					break;
				}
				c++;
				w += (double) _text->getFont()->getCharSize(*i).w / 2 * scaleX;
			}
			_caretPos = c;
		}
	}
	InteractiveSurface::mousePress(action, state);
}

/**
 * Changes the text edit according to keyboard input, and
 * unfocuses the text if Enter is pressed.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextEdit::keyboardPress(Action *action, State *state)
{
	if (_multiline && Options::keyboardMode == KEYBOARD_ON)
	{
		ensureMultilineLayout();
		if (!_multilineLayoutValid)
		{
			InteractiveSurface::keyboardPress(action, state);
			return;
		}
		bool changed = false;
		bool commitPressed = false;
		const SDL_Keycode key = action->getDetails()->key.keysym.sym;
		auto layout = [this]() -> const Calypso::CalypsoTextEditLayout& {
			return _multilineLayout;
		};
		switch (key)
		{
		case SDLK_LEFT:
			if (_caretPos > 0) --_caretPos;
			_preferredCaretX = -1;
			break;
		case SDLK_RIGHT:
			if (_caretPos < _value.length()) ++_caretPos;
			_preferredCaretX = -1;
			break;
		case SDLK_HOME:
			{
				const auto &l = layout();
				_caretPos = l.lines[Calypso::calypsoLocateTextEditCaret(l, _caretPos).line].start;
				_preferredCaretX = -1;
			}
			break;
		case SDLK_END:
			{
				const auto &l = layout();
				_caretPos = l.lines[Calypso::calypsoLocateTextEditCaret(l, _caretPos).line].end;
				_preferredCaretX = -1;
			}
			break;
		case SDLK_UP:
		case SDLK_DOWN:
			{
				const auto move = Calypso::calypsoMoveTextEditVertically(
					layout(), _caretPos, key == SDLK_UP ? -1 : 1, _preferredCaretX);
				_caretPos = move.position;
				_preferredCaretX = move.preferredX;
			}
			break;
		case SDLK_BACKSPACE:
			if (_caretPos > 0)
			{
				_value.erase(_caretPos - 1, 1);
				--_caretPos;
				changed = true;
			}
			_preferredCaretX = -1;
			break;
		case SDLK_DELETE:
			if (_caretPos < _value.length())
			{
				_value.erase(_caretPos, 1);
				changed = true;
			}
			_preferredCaretX = -1;
			break;
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			if (_enterPolicy == TEEP_COMMIT &&
				(action->getDetails()->key.keysym.mod & KMOD_SHIFT) == 0)
			{
				commitPressed = true;
			}
			else if (_textEditConstraint == TEC_NONE)
			{
				_value.insert(_caretPos++, 1, '\n');
				changed = true;
			}
			_preferredCaretX = -1;
			break;
		case SDLK_ESCAPE:
			// The owning state decides whether Escape cancels, closes, or is ignored.
			break;
		default:
			// SDL2 printable input arrives separately as SDL_TEXTINPUT.
			break;
		}
		if (changed) invalidateMultilineLayout();
		updateMultilineViewport();
		_redraw = true;
		if (changed && _change) (state->*_change)(action);
		if (commitPressed)
		{
			// Terminal callback: it may pop the owner and delete this TextEdit.
			// The commit key is owned here and is not dispatched a second time.
			commit(action);
			return;
		}
		InteractiveSurface::keyboardPress(action, state);
		return;
	}

	const UString previousValue = _value;
	bool enterPressed = false;
	if (Options::keyboardMode == KEYBOARD_OFF)
	{
		switch (action->getDetails()->key.keysym.sym)
		{
		case SDLK_UP:
			_char++;
			if (_char > '~')
			{
				_char = ' ';
			}
			break;
		case SDLK_DOWN:
			_char--;
			if (_char < ' ')
			{
				_char = '~';
			}
			break;
		case SDLK_LEFT:
			if (!_value.empty())
			{
				_value.resize(_value.length() - 1);
			}
			break;
		case SDLK_RIGHT:
			if (!exceedsMaxWidth(_char))
			{
				_value += _char;
			}
			break;
		default:
			break;
		}
	}
	else if (Options::keyboardMode == KEYBOARD_ON)
	{
		switch (action->getDetails()->key.keysym.sym)
		{
		case SDLK_LEFT:
			if (_caretPos > 0)
			{
				_caretPos--;
			}
			break;
		case SDLK_RIGHT:
			if (_caretPos < _value.length())
			{
				_caretPos++;
			}
			break;
		case SDLK_HOME:
			_caretPos = 0;
			break;
		case SDLK_END:
			_caretPos = _value.length();
			break;
		case SDLK_BACKSPACE:
			if (_caretPos > 0)
			{
				_value.erase(_caretPos - 1, 1);
				_caretPos--;
			}
			break;
		case SDLK_DELETE:
			if (_caretPos < _value.length())
			{
				_value.erase(_caretPos, 1);
			}
			break;
		case SDLK_ESCAPE:
			{
				_value = Unicode::convUtf8ToUtf32("");
				_caretPos = 0;
			}
			FALLTHROUGH;
			// no break; do the ENTER action too
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			if (!_value.empty() || _enter != 0)
			{
				enterPressed = true;
				setFocus(false);
			}
			break;
		default:
			// SDL2 printable input arrives separately as SDL_TEXTINPUT.
			break;
		}
	}
	_redraw = true;
	if (_change && _value != previousValue)
	{
		(state->*_change)(action);
	}
	if (_enter && enterPressed)
	{
		(state->*_enter)(action);
	}

	InteractiveSurface::keyboardPress(action, state);
}

/**
 * Sets a function to be called every time the text changes.
 * @param handler Action handler.
 */
void TextEdit::onChange(ActionHandler handler)
{
	_change = handler;
}

/**
* Sets a function to be called every time ENTER is pressed.
* @param handler Action handler.
*/
void TextEdit::onEnter(ActionHandler handler)
{
	_enter = handler;
}

void TextEdit::commit(Action *action)
{
	State *state = _state;
	ActionHandler enter = _enter;
	setFocus(false);
	if (!enter || !state) return;
	// Treat the callback as terminal: it may synchronously destroy the owner.
	(state->*enter)(action);
}

}
