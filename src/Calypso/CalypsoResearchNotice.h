#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoSmallConfirmationRenderer.h"

namespace OpenXcom {
class State;
class Window;
namespace Calypso {
class CalypsoResearchNotice final : public CalypsoHdFamilyAdapter
{
public:
	CalypsoResearchNotice(State *owner, const State *underlay, Mod *mod, Window *window,
		Text *message, std::string title, std::string context, TextButton *ok, TextButton *report = nullptr);
	~CalypsoResearchNotice() override;
	void layout();
	int designWidth() const { return _design.designWidth; }
	int designHeight() const { return _design.designHeight; }
	const void *topState() const override { return _owner; }
	const void *physicalUnderlayState() const override { return _underlay; }
	void collect(CalypsoHdFrameBuilder &builder) const override;
private:
	State *_owner;
	const State *_underlay;
	Mod *_mod;
	Window *_window;
	Text *_message;
	std::string _title, _context;
	TextButton *_ok, *_report;
	std::uint64_t _generation;
	CalypsoSmallConfirmationModel _design;
	mutable CalypsoSmallConfirmationMotion _motion;
};
} }
#endif
