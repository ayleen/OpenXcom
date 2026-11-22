#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoSmallConfirmationRenderer.h"
namespace OpenXcom
{
class State;
class Window;
namespace Calypso
{
class CalypsoStrategicList final : public CalypsoHdFamilyAdapter
{
public:
	CalypsoStrategicList(State *owner, const State *underlay, Mod *mod, Window *window,
		Text *title, TextList *list, TextButton *cancel, TextButton *keep = nullptr);
	~CalypsoStrategicList() override;
	void layout();
	int designWidth() const { return _design.designWidth; }
	int designHeight() const { return _design.designHeight; }
	const void *topState() const override { return _owner; }
	const void *physicalUnderlayState() const override { return _underlay; }
	void collect(CalypsoHdFrameBuilder &builder) const override;
private:
	std::uint64_t _harnessGeneration = 0;
	State *_owner;
	const State *_underlay;
	Mod *_mod;
	Window *_window;
	Text *_title;
	TextList *_list;
	TextButton *_cancel, *_keep;
	CalypsoScrollableCollectionModel _design;
	mutable CalypsoSmallConfirmationMotion _motion;
};
}
}
#endif
