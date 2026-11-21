#ifdef __EMSCRIPTEN__
#include "CalypsoF04SoldierBonusUi.h"
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
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldierBonusState.h"
#include "../Mod/Mod.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF04Soldiers.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF04SoldierBonusUi::BonusRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native rows only, mirroring the visible view: source rows carry their
// bonus rule names (stable keys, never translated text) and stay activatable;
// summary rows are inert aggregates (the native summary list owns no click
// handler) painted muted under adapter-internal positional ids.
CalypsoF04SoldierBonusUi::BonusRows CalypsoF04SoldierBonusUi::bonusRows(const SoldierBonusState& state)
{
	BonusRows out{};
	const bool sources = !state._lstSummary || state._lstSummary->getVisible() == false;
	const TextList* list = sources ? state._lstBonuses : state._lstSummary;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size());
	std::size_t row = 0;
	for (const auto& cells : matrix)
	{
		if (cells.empty() || !cells[0])
		{
			++row;
			continue;
		}
		CalypsoSelectionListRow entry{};
		if (sources)
		{
			out.ids.push_back(row < state._bonuses.size()
				? state._bonuses[row] : cells[0]->getText());
			entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
				cells[0]->getText());
			entry.enabled = true;
		}
		else
		{
			out.ids.push_back("summary-" + std::to_string(row));
			entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
				f04ComposeLabelValue(cells[0]->getText(),
					cells.size() > 1 && cells[1] ? cells[1]->getText() : std::string()));
			entry.enabled = false;
		}
		out.rows.push_back(entry);
		++row;
	}
	return out;
}

TextList* CalypsoF04SoldierBonusUi::bonusActiveList(SoldierBonusState& state)
{
	if (state._lstSummary && state._lstSummary->getVisible()) return state._lstSummary;
	return state._lstBonuses;
}

CalypsoF04SoldierBonusUi::~CalypsoF04SoldierBonusUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF04SoldierBonusUi::topState() const
{
	return _state;
}

void CalypsoF04SoldierBonusUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_btnSummary);
	suppression.add(_state->_btnCancel);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtType);
	suppression.add(_state->_lstBonuses);
	suppression.add(_state->_lstSummary);
}

void CalypsoF04SoldierBonusUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The bonuses route is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 bonus prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 bonus generated layout is missing");

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return f04ProjectRect(window, *generated, rect);
	};

	CalypsoSelectionListModel model{};
	model.familyId = CalypsoF04SoldiersGen::kFamilyId;
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
	TextList* const active = _state->_lstSummary && _state->_lstSummary->getVisible()
		? _state->_lstSummary : _state->_lstBonuses;
	model.listWidget = active;
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.protocolText = CalypsoF04SoldiersGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (active && active->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = active->getCalypsoHdTrackRect();
		const SDL_Rect thumb = active->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF04SoldiersGen::kRowSlotWideCount
		: CalypsoF04SoldiersGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF04SoldiersGen::kRowSlotsWide
		: CalypsoF04SoldiersGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order/availability: source rows activate the
	// Stats-for-Nerds article through the unchanged native handler; summary
	// rows are inert views of the same engine diary data.
	const BonusRows bound = bonusRows(*_state);
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (active)
	{
		model.scrollOffset = active->getScroll();
		const unsigned int selected = active->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF04SoldiersGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF04SoldiersGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF04SoldiersGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF04SoldiersGen::kPanelFillBottom;
	model.frameColor = CalypsoF04SoldiersGen::kFrame;
	model.protocolColor = CalypsoF04SoldiersGen::kProtocolText;
	model.dividerColor = CalypsoF04SoldiersGen::kDivider;
	model.footerDotColor = CalypsoF04SoldiersGen::kFooterDot;
	model.textColor = CalypsoF04SoldiersGen::kText;
	model.mutedTextColor = CalypsoF04SoldiersGen::kMutedText;
	model.selectionColor = CalypsoF04SoldiersGen::kSelection;
	model.scrollTrackColor = CalypsoF04SoldiersGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF04SoldiersGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF04SoldiersGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF04SoldiersGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF04SoldiersGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF04SoldiersGen::kButtons[0];
	TextButton* widget = _state->_btnCancel;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f04GeneratedButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF04SoldierBonusUi::applyGeneratedLayout(SoldierBonusState& state, bool wide)
{
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	// Both views share the list slot; the toggle swaps native visibility and
	// the adapter mirrors whichever list is visible each frame.
	f04ApplyRect(state._lstBonuses, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	f04ApplyRect(state._lstSummary, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f04GeneratedButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnCancel, touch);
	}
	// The Sources/Summary toggle stays a live native input owner (handler and
	// toggle state untouched) without painted controls or pointer hit areas.
	f04ParkOffscreen(state._btnSummary);
	f04ParkOffscreen(state._txtType);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		const int minimum = std::max(fontH, generated->rowHeight - spacing);
		if (state._lstBonuses) state._lstBonuses->setMinimumRowHeight(minimum);
		if (state._lstSummary) state._lstSummary->setMinimumRowHeight(minimum);
	}
}

void CalypsoF04SoldierBonusUi::configure(SoldierBonusState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F04"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f04HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F04 bonus generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f04SeamSelectionList(state._lstBonuses, state._window, *generated);
	f04SeamSelectionList(state._lstSummary, state._window, *generated);
	auto* adapter = new CalypsoF04SoldierBonusUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF04SoldierBonusUi::resize(SoldierBonusState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable id BEFORE re-layout (pitfall 3), then
	// restore-or-clamp it into the relaid-out rows (never row numbers).
	const BonusRows before = bonusRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		bonusActiveList(state), before.ids);
	const bool wide = f04HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF04SoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	f04SeamSelectionList(state._lstBonuses, state._window, *generated);
	f04SeamSelectionList(state._lstSummary, state._window, *generated);
	const BonusRows after = bonusRows(state);
	f04RestoreListSelection(bonusActiveList(state), captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
