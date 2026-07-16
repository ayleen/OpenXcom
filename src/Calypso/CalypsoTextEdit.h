#pragma once

#ifdef __EMSCRIPTEN__

namespace OpenXcom
{
class Action;
class State;
class TextEdit;

namespace Calypso
{

/// Browser-only multiline/SDL_TEXTINPUT adapter. TextEdit keeps the native
/// legacy path while delegating the Calypso-specific orchestration here.
class CalypsoTextEdit
{
private:
	static void ensureLayout(TextEdit& edit);
	static void updateViewport(TextEdit& edit);
public:
	static void invalidateLayout(TextEdit& edit);
	static void assignText(TextEdit& edit, const char* utf8);
	static void setMultiline(TextEdit& edit, bool multiline);
	static void resized(TextEdit& edit);
	static bool hasEffectivePalette(const TextEdit& edit);
	static void paletteChanged(TextEdit& edit, bool hadEffectivePalette);
	static void textInput(TextEdit& edit, Action* action, State* state);
	static bool draw(TextEdit& edit);
	static bool mousePress(TextEdit& edit, Action* action, State* state);
	static bool keyboardPress(TextEdit& edit, Action* action, State* state);
	static void setTextExternal(TextEdit& edit, const char* utf8);
	static void refreshExternalGeometry(TextEdit& edit);
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
