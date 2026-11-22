#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom {
class State;
class Window;
namespace Calypso {
enum class ContactNoticeType { TrackingLost, AutomaticRecovery, TacticalAlert, TacticalNotice, BaseDiscovered };

class CalypsoContactNotice final : public CalypsoHdFamilyAdapter
{
public:
	CalypsoContactNotice(ContactNoticeType type, State *owner, const State *underlay,
		Mod *mod, Window *window, Text *message, TextButton *action,
		const std::string &title);
	~CalypsoContactNotice() override;
	void layout();
	int designWidth() const { return _design.designWidth; }
	int designHeight() const { return _design.designHeight; }
	const void *topState() const override { return _owner; }
	const void *physicalUnderlayState() const override { return _underlay; }
	void collect(CalypsoHdFrameBuilder &builder) const override;
private:
	ContactNoticeType _type;
	State *_owner;
	const State *_underlay;
	Mod *_mod;
	Window *_window;
	Text *_message;
	TextButton *_action;
	std::string _title;
	std::uint64_t _generation;
	CalypsoSmallConfirmationModel _design;
	mutable CalypsoSmallConfirmationMotion _motion;
};
} }
#endif
