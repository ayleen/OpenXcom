#pragma once
/*
 * Shared full-screen HD renderer. Generated screen contracts are mapped into
 * this archetype-neutral model by the owning state or harness fixture; the
 * renderer never branches on a screen identity and never owns behavior.
 */
#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>

#include "CalypsoHdFamilyAdapter.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoHdScreenRect
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

struct CalypsoHdScreenActionVisual
{
	std::string id;
	std::string label;
	std::string component;
	std::string slotRole;
	std::string coordinateSpace;
	CalypsoHdScreenRect visible;
	int focusOrder = 0;
	int zOrder = 1;
};

struct CalypsoHdScreenRegionVisual
{
	std::string id;
	CalypsoHdScreenRect rect;
};

struct CalypsoHdScreenCopy
{
	std::string key;
	std::string value;
};

struct CalypsoHdScreenRenderModel
{
	std::string archetype;
	int designWidth = 0;
	int designHeight = 0;
	bool sideBySidePreview = false;
	std::vector<CalypsoHdScreenActionVisual> actions;
	std::vector<CalypsoHdScreenRegionVisual> regions;
	std::vector<CalypsoHdScreenCopy> copy;
	std::string selectedActionId;
};

class CalypsoHdScreenRenderer : public CalypsoHdFamilyAdapter
{
public:
	CalypsoHdScreenRenderer(const void* state, CalypsoHdScreenRenderModel model);
	~CalypsoHdScreenRenderer() override;

	const void* topState() const override;
	void collect(CalypsoHdFrameBuilder& builder) const override;
	void setModel(CalypsoHdScreenRenderModel model);

private:
	const void* _state;
	CalypsoHdScreenRenderModel _model;
};

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
