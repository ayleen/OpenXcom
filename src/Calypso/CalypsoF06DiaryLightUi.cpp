#ifdef __EMSCRIPTEN__
#include "CalypsoF06DiaryLightUi.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "../Engine/Font.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldierDiaryLightState.h"
#include "../Mod/Mod.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF06DiaryLight.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF06DiaryLightUi::LightRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native rows only, in native order: exact weapon neutralization
// breakdowns (translated weapon name + count verbatim). Stable ids are the
// weapon rule strings (never translated text). Rows are inert views: the
// native list owns no click handler and the state is read-only.
CalypsoF06DiaryLightUi::LightRows CalypsoF06DiaryLightUi::lightRows(const SoldierDiaryLightState& state)
{
	LightRows out{};
	const TextList* list = state._lstStats;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	for (size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		const std::string label = cells[0]->getText();
		const std::string value = cells.size() > 1 && cells[1]
			? cells[1]->getText() : std::string();
		out.ids.push_back("weapon-" + std::to_string(row));
		CalypsoSelectionListRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeLabelValue(label, value));
		entry.enabled = false;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF06DiaryLightUi::~CalypsoF06DiaryLightUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF06DiaryLightUi::topState() const
{
	return _state;
}

void CalypsoF06DiaryLightUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_lstStats);
	suppression.add(_state->_btnOk);
}

void CalypsoF06DiaryLightUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The combat-totals route is a registered HD route: missing prerequisites
	// or a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 combat prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f06DiaryLightLayout(wide);

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return f06ProjectRect(window, *generated, rect);
	};

	CalypsoSelectionListModel model{};
	model.familyId = CalypsoF06DiaryLightGen::kFamilyId;
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
	model.listWidget = _state->_lstStats;
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.protocolText = CalypsoF06DiaryLightGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstStats && _state->_lstStats->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstStats->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstStats->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF06DiaryLightGen::kRowSlotWideCount
		: CalypsoF06DiaryLightGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF06DiaryLightGen::kRowSlotsWide
		: CalypsoF06DiaryLightGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Inert views of the exact native weapon totals; the native list owns
	// scroll while OK pops back to the Battlescape/inventory entry context.
	// No selection applies to the combat record.
	const LightRows bound = lightRows(*_state);
	model.rows = bound.rows;
	if (_state->_lstStats)
		model.scrollOffset = _state->_lstStats->getScroll();
	model.hasSelection = false;
	model.selectedRow = 0;

	model.cutCornerPx = CalypsoF06DiaryLightGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF06DiaryLightGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF06DiaryLightGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF06DiaryLightGen::kPanelFillBottom;
	model.frameColor = CalypsoF06DiaryLightGen::kFrame;
	model.protocolColor = CalypsoF06DiaryLightGen::kProtocolText;
	model.dividerColor = CalypsoF06DiaryLightGen::kDivider;
	model.footerDotColor = CalypsoF06DiaryLightGen::kFooterDot;
	model.textColor = CalypsoF06DiaryLightGen::kText;
	model.mutedTextColor = CalypsoF06DiaryLightGen::kMutedText;
	model.selectionColor = CalypsoF06DiaryLightGen::kSelection;
	model.scrollTrackColor = CalypsoF06DiaryLightGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF06DiaryLightGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF06DiaryLightGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF06DiaryLightGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF06DiaryLightGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF06DiaryLightGen::kButtons[0];
	TextButton* widget = _state->_btnOk;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f06DiaryLightButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF06DiaryLightUi::applyGeneratedLayout(SoldierDiaryLightState& state, bool wide)
{
	const auto* generated = CalypsoF06DiaryLightGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstStats, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f06DiaryLightButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstStats && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstStats->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF06DiaryLightUi::configure(SoldierDiaryLightState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F06"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f06HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF06DiaryLightGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F06 combat generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f06SeamSelectionList(state._lstStats, state._window, *generated);
	auto* adapter = new CalypsoF06DiaryLightUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF06DiaryLightUi::resize(SoldierDiaryLightState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = f06HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF06DiaryLightGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it. The combat record carries no selection.
	f06SeamSelectionList(state._lstStats, state._window, *generated);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
