#ifdef __EMSCRIPTEN__
#include "CalypsoF05SoldierTransformUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "../Engine/Font.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/ArrowButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldierTransformState.h"
#include "../Mod/Mod.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF05SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF05TransformSelect.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF05SoldierTransformUi::TransformRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native rows only, in native order: the translated project names
// exactly as the native cells show them. Stable ids are the transformation
// rule types (never translated text); eligibility stays engine-authoritative
// (the native ctor already filters by isEligibleForTransformation).
CalypsoF05SoldierTransformUi::TransformRows CalypsoF05SoldierTransformUi::transformRows(const SoldierTransformState& state)
{
	TransformRows out{};
	const TextList* list = state._lstTransformations;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size());
	for (size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		std::string id = texts[0];
		if (row < state._indices.size())
		{
			const int native = state._indices[row];
			if (native >= 0 && (size_t)native < state._transformations.size())
				id = state._transformations[(size_t)native].type;
		}
		out.ids.push_back(id);
		CalypsoSelectionListRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF05SoldierTransformUi::~CalypsoF05SoldierTransformUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF05SoldierTransformUi::topState() const
{
	return _state;
}

void CalypsoF05SoldierTransformUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_btnCancel);
	suppression.add(_state->_btnQuickSearch);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtType);
	suppression.add(_state->_lstTransformations);
	suppression.add(_state->_sortName);
}

void CalypsoF05SoldierTransformUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The project selector is a registered HD route: missing prerequisites
	// or a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed
	// anyway). Selection never mutates: row activation pushes the
	// SoldierTransformationState review through the unchanged native handler.
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 transform prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f05TransformSelectLayout(wide);

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return f05ProjectRect(window, *generated, rect);
	};

	CalypsoSelectionListModel model{};
	model.familyId = CalypsoF05TransformSelectGen::kFamilyId;
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
	model.listWidget = _state->_lstTransformations;
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.protocolText = CalypsoF05TransformSelectGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstTransformations && _state->_lstTransformations->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstTransformations->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstTransformations->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF05TransformSelectGen::kRowSlotWideCount
		: CalypsoF05TransformSelectGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF05TransformSelectGen::kRowSlotsWide
		: CalypsoF05TransformSelectGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order/eligibility: left-click pushes the review
	// through the unchanged native handler (non-mutating); middle-click opens
	// the project article through the unchanged shortcut. The native list
	// stays the behavior/input owner for both paths.
	const TransformRows bound = transformRows(*_state);
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (_state->_lstTransformations)
	{
		model.scrollOffset = _state->_lstTransformations->getScroll();
		const unsigned int selected = _state->_lstTransformations->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF05TransformSelectGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF05TransformSelectGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF05TransformSelectGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF05TransformSelectGen::kPanelFillBottom;
	model.frameColor = CalypsoF05TransformSelectGen::kFrame;
	model.protocolColor = CalypsoF05TransformSelectGen::kProtocolText;
	model.dividerColor = CalypsoF05TransformSelectGen::kDivider;
	model.footerDotColor = CalypsoF05TransformSelectGen::kFooterDot;
	model.textColor = CalypsoF05TransformSelectGen::kText;
	model.mutedTextColor = CalypsoF05TransformSelectGen::kMutedText;
	model.selectionColor = CalypsoF05TransformSelectGen::kSelection;
	model.scrollTrackColor = CalypsoF05TransformSelectGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF05TransformSelectGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF05TransformSelectGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF05TransformSelectGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF05TransformSelectGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF05TransformSelectGen::kButtons[0];
	TextButton* widget = _state->_btnCancel;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f05TransformSelectButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF05SoldierTransformUi::applyGeneratedLayout(SoldierTransformState& state, bool wide)
{
	const auto* generated = CalypsoF05TransformSelectGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstTransformations, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f05TransformSelectButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnCancel, touch);
	}
	// Type/sort headers have no painted equivalent in the shared shell; the
	// composed rows carry the native project names verbatim.
	f04ParkOffscreen(state._txtType);
	f04ParkOffscreen(state._sortName);
	// Quick search stays a live native input owner (toggle/apply handlers and
	// the oxceQuickSearchButton gate untouched) without painted controls.
	f04ParkOffscreen(state._btnQuickSearch);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstTransformations && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstTransformations->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF05SoldierTransformUi::configure(SoldierTransformState& state, bool allow)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!allow || !state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F05"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f05HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF05TransformSelectGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 transform generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f05SeamSelectionList(state._lstTransformations, state._window, *generated);
	auto* adapter = new CalypsoF05SoldierTransformUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF05SoldierTransformUi::resize(SoldierTransformState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable id BEFORE re-layout (pitfall 3), then
	// restore-or-clamp it into the relaid-out rows (never row numbers).
	const TransformRows before = transformRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstTransformations, before.ids);
	const bool wide = f05HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF05TransformSelectGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	f05SeamSelectionList(state._lstTransformations, state._window, *generated);
	const TransformRows after = transformRows(state);
	f04RestoreListSelection(state._lstTransformations, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
