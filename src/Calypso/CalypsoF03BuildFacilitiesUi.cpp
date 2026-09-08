#ifdef __EMSCRIPTEN__
#include "CalypsoF03BuildFacilitiesUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Engine/Font.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Basescape/BuildFacilitiesState.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF03BuildFacilities.generated.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
#include "CalypsoUiMetrics.h"
#include "CalypsoViewportRuntime.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

CalypsoLayoutClass currentLayoutClass()
{
	const CalypsoViewportRuntime& runtime = calypsoViewportRuntime();
	if (!runtime.hasLayout())
		CalypsoHdUiOverlay::instance().failHdRoute("F03 chooser viewport layout is unavailable");
	// Layout class is a CSS-logical policy decision. Projecting into the
	// engine base framebuffer (320x180 logical) collapses the Live1280x720
	// safe area and always selects Compact on an otherwise Wide viewport.
	const CalypsoLayoutMetrics& metrics = runtime.current();
	if (metrics.logicalWidth <= 0 || metrics.logicalHeight <= 0 ||
		metrics.safeWidth <= 0 || metrics.safeHeight <= 0)
		CalypsoHdUiOverlay::instance().failHdRoute("F03 chooser viewport layout is unavailable");
	const CalypsoBaseSafeRect safe{metrics.safeX, metrics.safeY, metrics.safeWidth, metrics.safeHeight};
	return calypsoHarnessEffectiveLayout(calypsoHarnessSession(), safe);
}

CalypsoLogicalRect shiftedRect(
	const CalypsoF03BuildFacilitiesGen::CalypsoF03BuildFacilitiesGenRect& rect, int dx)
{
	return {rect.x + dx, rect.y, rect.w, rect.h};
}

void applyRect(Surface* surface, const CalypsoLogicalRect& rect)
{
	if (!surface) return;
	surface->setX(rect.x);
	surface->setY(rect.y);
	surface->setWidth(rect.w);
	surface->setHeight(rect.h);
}

int presentationShiftX(
	const CalypsoF03BuildFacilitiesGen::CalypsoF03BuildFacilitiesGenLayout& layout, bool wide)
{
	return calypsoHarnessSession().sideBySide && wide ? 40 - layout.window.x : 0;
}

CalypsoLogicalRect generatedButtonRect(bool wide, const char* id, int dx)
{
	const auto& buttons = CalypsoF03BuildFacilitiesGen::kButtonRects[wide ? 0 : 1];
	for (int i = 0; i < CalypsoF03BuildFacilitiesGen::kButtonCount; ++i)
	{
		if (std::string(buttons[i].id) == id) return shiftedRect(buttons[i].rect, dx);
	}
	return {};
}

CalypsoLogicalRect touchRect(CalypsoLogicalRect visual)
{
	const int width = std::max(visual.w, CALYPSO_MIN_TOUCH_TARGET);
	const int height = std::max(visual.h, CALYPSO_MIN_TOUCH_TARGET);
	visual.x -= width - visual.w;
	visual.y -= (height - visual.h) / 2;
	visual.w = width;
	visual.h = height;
	return visual;
}

/// Configures the native HD selection-list seam AFTER enableUiScaling /
/// recapture, from the actual projected list rect and the generated metrics.
/// Same values re-applied preserve drag capture; real changes reset it.
/// rowStride projects generated.rowHeight by the SAME uiScale so native scroll
/// count/reveal agrees with the painted generated rowSlots (visibleRows).
void configureHdSelectionList(
	TextList* list,
	Window* window,
	const CalypsoF03BuildFacilitiesGen::CalypsoF03BuildFacilitiesGenLayout& generated)
{
	if (!list || !window || generated.window.w <= 0) return;
	const double uiScale = (double)window->getWidth() / (double)generated.window.w;
	const int scrollBarWidth = std::max(1, (int)std::llround(generated.scrollBarWidth * uiScale));
	const int minThumbHeight = std::max(1, (int)std::llround(generated.minThumbHeight * uiScale));
	const int rowStride = std::max(1, (int)std::llround(generated.rowHeight * uiScale));
	const size_t visibleRows = generated.visibleRows > 0 ? (size_t)generated.visibleRows : 0;
	list->configureCalypsoHdSelectionList(scrollBarWidth, minThumbHeight, rowStride, visibleRows);
}

} // namespace

CalypsoF03BuildFacilitiesUi::~CalypsoF03BuildFacilitiesUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF03BuildFacilitiesUi::topState() const
{
	return _state;
}

const void* CalypsoF03BuildFacilitiesUi::physicalUnderlayState() const
{
	// The chooser is a transparent form over the live physical base: compose
	// the covered base owner below so the base shows through instead of black.
	if (!_state) return nullptr;
	return _state->_state;
}

void CalypsoF03BuildFacilitiesUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_lstFacilities);
	suppression.add(_state->_btnOk);
}

void CalypsoF03BuildFacilitiesUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The browser chooser form is mandatory HD: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F03 chooser prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = CalypsoF03BuildFacilitiesGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F03 chooser generated layout is missing");

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return {
			window.x + (int)std::llround((rect.x - generated->window.x) * uiScale),
			window.y + (int)std::llround((rect.y - generated->window.y) * uiScale),
			std::max(1, (int)std::llround(rect.w * uiScale)),
			std::max(1, (int)std::llround(rect.h * uiScale))};
	};

	CalypsoSelectionListModel model{};
	model.familyId = CalypsoF03BuildFacilitiesGen::kFamilyId;
	model.instance = _state;
	model.mod = _state->_game->getMod();
	model.wide = wide;
	model.designWidth = generated->designWidth;
	model.designHeight = generated->designHeight;
	model.window = window;
	model.status = project(generated->status);
	model.title = project(generated->title);
	model.list = project(generated->list);
	model.footer = project(generated->footer);
	model.windowWidget = _state->_window;
	model.titleWidget = _state->_txtTitle;
	model.listWidget = _state->_lstFacilities;
	model.titleText = _state->_txtTitle ? _state->_txtTitle->getText() : std::string();
	model.protocolText = CalypsoF03BuildFacilitiesGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstFacilities && _state->_lstFacilities->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstFacilities->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstFacilities->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF03BuildFacilitiesGen::kRowSlotWideCount
		: CalypsoF03BuildFacilitiesGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF03BuildFacilitiesGen::kRowSlotsWide
		: CalypsoF03BuildFacilitiesGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order/availability: enabled rules first, then the
	// disabled tail, exactly as populateBuildList() appends them.
	const std::size_t enabled = _state->_facilities.size();
	const std::size_t total = enabled + _state->_disabledFacilities.size();
	for (std::size_t row = 0; row < total; ++row)
	{
		CalypsoSelectionListRow entry{};
		entry.text = _state->_lstFacilities
			? _state->_lstFacilities->getCellText(row, 0) : std::string();
		entry.enabled = row < enabled;
		model.rows.push_back(entry);
	}
	if (_state->_lstFacilities)
	{
		model.scrollOffset = _state->_lstFacilities->getScroll();
		const unsigned int selected = _state->_lstFacilities->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF03BuildFacilitiesGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF03BuildFacilitiesGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF03BuildFacilitiesGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF03BuildFacilitiesGen::kPanelFillBottom;
	model.frameColor = CalypsoF03BuildFacilitiesGen::kFrame;
	model.protocolColor = CalypsoF03BuildFacilitiesGen::kProtocolText;
	model.dividerColor = CalypsoF03BuildFacilitiesGen::kDivider;
	model.footerDotColor = CalypsoF03BuildFacilitiesGen::kFooterDot;
	model.textColor = CalypsoF03BuildFacilitiesGen::kText;
	model.mutedTextColor = CalypsoF03BuildFacilitiesGen::kMutedText;
	model.selectionColor = CalypsoF03BuildFacilitiesGen::kSelection;
	model.scrollTrackColor = CalypsoF03BuildFacilitiesGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF03BuildFacilitiesGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF03BuildFacilitiesGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF03BuildFacilitiesGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF03BuildFacilitiesGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF03BuildFacilitiesGen::kButtons[0];
	TextButton* widget = _state->_btnOk;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = widget ? widget->getText() : std::string();
	model.cancel.rect = project(generatedButtonRect(wide, generatedButton.id, 0));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF03BuildFacilitiesUi::applyGeneratedLayout(
	BuildFacilitiesState& state, bool wide)
{
	const auto* generated = CalypsoF03BuildFacilitiesGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	const int dx = presentationShiftX(*generated, wide);
	applyRect(state._window, shiftedRect(generated->window, dx));
	applyRect(state._txtTitle, shiftedRect(generated->title, dx));
	applyRect(state._lstFacilities, shiftedRect(generated->list, dx));
	applyRect(state._btnOk, touchRect(generatedButtonRect(wide, "cancel", dx)));
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstFacilities && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstFacilities->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF03BuildFacilitiesUi::configure(BuildFacilitiesState& state)
{
	// The browser chooser form is mandatory HD: no allow flag, no locale gate,
	// no family toggle. Missing prerequisites or a missing generated layout
	// fails the route closed instead of silently falling back to vanilla.
	// The footer's native label/callback (Cancel, or Reset in the subclass)
	// are never touched here; collect() reads the live native label.
	if (state._hdAdapter)
	{
		CalypsoHdUiOverlay::instance().clearAdapter(state._hdAdapter);
		delete state._hdAdapter;
		state._hdAdapter = nullptr;
		state._hdLayout = false;
	}
	if (!state._game || !state._game->getMod() || !state._game->getLanguage())
		CalypsoHdUiOverlay::instance().failHdRoute("F03 chooser prerequisites are unavailable");
	state._hdLayout = true;

	state._hdWideLayout = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdHarnessGeneration = Calypso::calypsoHarnessSession().generation;
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF03BuildFacilitiesGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F03 chooser generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	configureHdSelectionList(state._lstFacilities, state._window, *generated);
	auto* adapter = new CalypsoF03BuildFacilitiesUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

bool CalypsoF03BuildFacilitiesUi::resize(BuildFacilitiesState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = currentLayoutClass() == CalypsoLayoutClass::Wide;
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF03BuildFacilitiesGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	configureHdSelectionList(state._lstFacilities, state._window, *generated);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
