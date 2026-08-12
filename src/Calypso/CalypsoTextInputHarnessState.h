#pragma once

#ifdef __EMSCRIPTEN__

#include "../Engine/State.h"

namespace OpenXcom
{

class TextEdit;

/// GPL-only interactive state used by browser regression tests to exercise the
/// real DOM -> SDL -> TextEdit path without TFTD assets.
class CalypsoTextInputHarnessState final : public State
{
private:
	TextEdit *_edit;
	int _changeCount;
	int _transitionCount;
	int _terminalKeyCount;
	int _lastTerminalKey;

public:
	explicit CalypsoTextInputHarnessState(bool multiline);
	void init() override;
	void handle(Action *action) override;
	void editChanged(Action *action);
	void editCommitted(Action *action);

	std::string value() const;
	int changeCount() const { return _changeCount; }
	int transitionCount() const { return _transitionCount; }
	int terminalKeyCount() const { return _terminalKeyCount; }
	int lastTerminalKey() const { return _lastTerminalKey; }
};

} // namespace OpenXcom

#endif /* __EMSCRIPTEN__ */
