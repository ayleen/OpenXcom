#ifdef __EMSCRIPTEN__
#include "CalypsoF07CraftPilotsUi.h"
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
#include "../Basescape/CraftPilotsState.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCraft.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "CalypsoF07SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF07CraftPilots.generated.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

std::string f07PilotsLiveText(const Text* single)
{
	return CommandCenter::calypsoHdNormalizeTtfDisplayText(
		single ? single->getText() : std::string());
}


} // namespace

// Assigned pilots in native order, composed verbatim from the live native
// list cells. The native list binds no click handler: rows are an
// inspector, never an action. Add/Clear live on their own generated action
// slots now (no flattened action rows).
std::vector<CalypsoTabbedRow> CalypsoF07CraftPilotsUi::craftPilotsRows(const CraftPilotsState& state)
{
	std::vector<CalypsoTabbedRow> rows;
	if (!state._lstPilots) return rows;
	const auto& matrix = state._lstPilots->getCellTextsSnapshot();
	for (const auto& cells : matrix)
	{
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		CalypsoTabbedRow row{};
		row.cells.reserve(CalypsoF07CraftPilotsGen::kColumnCount);
		for (int i = 0; i < CalypsoF07CraftPilotsGen::kColumnCount; ++i)
		{
			row.cells.push_back(CommandCenter::calypsoHdNormalizeTtfDisplayText(
				cells[i] ? cells[i]->getText() : std::string()));
		}
		row.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		row.enabled = false;
		if (!row.text.empty()) rows.push_back(row);
	}
	return rows;
}

// Live seat counts: assigned rows from the native list, required seats from
// the craft rules. Both feed the assigned summary and the detail subtitle
// as language-neutral counts.
void CalypsoF07CraftPilotsUi::craftPilotsSeats(const CraftPilotsState& state, std::size_t& assigned, int& required)
{
	assigned = 0;
	required = 0;
	if (state._lstPilots) assigned = state._lstPilots->getTexts();
	const std::vector<Craft*>* crafts = state._base ? state._base->getCrafts() : nullptr;
	if (crafts && state._craft < crafts->size() && (*crafts)[state._craft]
		&& (*crafts)[state._craft]->getRules())
		required = (*crafts)[state._craft]->getRules()->getPilots();
}

CalypsoF07CraftPilotsUi::~CalypsoF07CraftPilotsUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF07CraftPilotsUi::topState() const
{
	return _state;
}

void CalypsoF07CraftPilotsUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtFiringAcc);
	suppression.add(_state->_txtReactions);
	suppression.add(_state->_txtBravery);
	suppression.add(_state->_txtPilots);
	suppression.add(_state->_lstPilots);
	suppression.add(_state->_txtRequired);
	suppression.add(_state->_txtAccuracyBonus);
	suppression.add(_state->_txtAccuracyBonusValue);
	suppression.add(_state->_txtDodgeBonus);
	suppression.add(_state->_txtDodgeBonusValue);
	suppression.add(_state->_txtApproachSpeed);
	suppression.add(_state->_txtApproachSpeedValue);
	suppression.add(_state->_btnAdd);
	suppression.add(_state->_btnRemoveAll);
	suppression.add(_state->_btnOk);
}

void CalypsoF07CraftPilotsUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The pilots screen is a registered HD route: missing prerequisites or
	// a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed
	// anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 pilots prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f07CraftPilotsLayout(wide);

	const CalypsoLogicalRect window = _state->_window
		? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(),
			_state->_window->getWidth(), _state->_window->getHeight()}
		: CalypsoLogicalRect{};
	const double uiScale = generated->window.w > 0
		? (double)window.w / generated->window.w : 1.0;
	auto project = [&](const auto& rect) -> CalypsoLogicalRect
	{
		return calypsoTabbedProjectRect(window, *generated, rect);
	};
	auto normalize = [](const std::string& text) -> std::string
	{
		return CommandCenter::calypsoHdNormalizeTtfDisplayText(text);
	};

	CalypsoTabbedModel model{};
	model.familyId = CalypsoF07CraftPilotsGen::kFamilyId;
	model.instance = _state;
	model.mod = _state->_game->getMod();
	model.wide = wide;
	model.designWidth = generated->designWidth;
	model.designHeight = generated->designHeight;
	model.window = window;
	model.title = project(generated->title);
	model.summaryBar = project(generated->summaryBar);
	model.tabBar = project(generated->tabBar);
	model.toolbarBar = project(generated->toolbarBar);
	model.collectionViewport = project(generated->collectionViewport);
	model.detailPanel = project(generated->detailPanel);
	model.footer = project(generated->footer);
	model.windowWidget = _state->_window;
	model.titleWidget = _state->_txtTitle;
	// The native pilot list stays the behavior/input owner (scroll position
	// and selection); its rows bind no click handler natively and commit
	// nothing.
	model.listWidget = _state->_lstPilots;
	model.titleText = normalize(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstPilots && _state->_lstPilots->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstPilots->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstPilots->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	for (int i = 0; i < generated->visibleRows; ++i)
		model.rowSlots.push_back(project(CalypsoF07CraftPilotsGen::kRowSlots[wide ? 0 : 1][i].rect));

	// Section tab: the pilots screen is a leaf route with no native tab
	// controls, so only the current pilots tab paints (selected,
	// display-only) with its localized section name.
	{
		CalypsoTabbedTab pilots{};
		pilots.id = "pilots";
		pilots.label = normalize(_state->tr("STR_PILOTS"));
		pilots.selected = true;
		pilots.enabled = true;
		pilots.widget = nullptr;
		pilots.rect = project(f07CraftPilotsTabRect(wide, "pilots"));
		if (!pilots.label.empty()) model.tabs.push_back(pilots);
	}

	// Summary: required verbatim from its native text, assigned as the live
	// "n / m" occupancy (language-neutral counts, matching the approved
	// slot semantics). Eligible/status have no native owner and stay
	// unpainted rather than stale.
	{
		std::size_t assigned = 0;
		int required = 0;
		craftPilotsSeats(*_state, assigned, required);
		CalypsoTabbedSummaryField requiredField{};
		requiredField.text = f07PilotsLiveText(_state->_txtRequired);
		requiredField.rect = project(calypsoTabbedFindRect(
			CalypsoF07CraftPilotsGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF07CraftPilotsGen::kSummaryCount, "required"));
		if (!requiredField.text.empty()) model.summary.push_back(requiredField);
		if (required > 0)
		{
			CalypsoTabbedSummaryField assignedField{};
			assignedField.text = std::to_string(assigned) + " / " + std::to_string(required);
			assignedField.rect = project(calypsoTabbedFindRect(
				CalypsoF07CraftPilotsGen::kSummaryRects[wide ? 0 : 1],
				CalypsoF07CraftPilotsGen::kSummaryCount, "assigned"));
			model.summary.push_back(assignedField);
		}
	}

	// Collection columns from the live native header texts; the seat column
	// has no native header and paints no label.
	for (int i = 0; i < CalypsoF07CraftPilotsGen::kColumnCount; ++i)
	{
		const std::string columnId = CalypsoF07CraftPilotsGen::kColumnIds[i]
			? CalypsoF07CraftPilotsGen::kColumnIds[i] : "";
		const Text* header = nullptr;
		if (columnId == "diver") header = _state->_txtPilots;
		else if (columnId == "firing") header = _state->_txtFiringAcc;
		else if (columnId == "reactions") header = _state->_txtReactions;
		else if (columnId == "bravery") header = _state->_txtBravery;
		CalypsoTabbedColumn column{};
		column.label = header ? normalize(header->getText()) : std::string();
		column.rect = project(calypsoTabbedFindRect(
			CalypsoF07CraftPilotsGen::kColumnHeaders[wide ? 0 : 1],
			CalypsoF07CraftPilotsGen::kColumnCount, columnId.c_str()));
		if (!column.label.empty()) model.columns.push_back(column);
	}

	// Native population/order: every assigned pilot in list order.
	const std::vector<CalypsoTabbedRow> bound = craftPilotsRows(*_state);
	model.rows = bound;
	const std::size_t rowsTotal = model.rows.size();
	if (_state->_lstPilots)
	{
		model.scrollOffset = _state->_lstPilots->getScroll();
		const unsigned int selected = _state->_lstPilots->getSelectedRow();
		model.hasSelection = selected < rowsTotal;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	// Pilot-bonus detail from the live native label/value pairs (the
	// calculated accuracy/dodge % bonuses and the approach-speed modifier,
	// localized). The headline accuracy pair doubles as the detail title;
	// the subtitle carries the live seat occupancy. The Clear action paints
	// exactly while the native Remove All button is visible under the
	// approved readout correction; otherwise the slot stays empty and the
	// widget stays parked (never a painted dead-end, never a parked hit
	// area).
	{
		model.detail.present = true;
		model.detail.panel = project(generated->detailPanel);
		model.detail.titleRect = project(CalypsoF07CraftPilotsGen::kDetailTitleRects[wide ? 0 : 1]);
		model.detail.subtitleRect = project(CalypsoF07CraftPilotsGen::kDetailSubtitleRects[wide ? 0 : 1]);
		const std::string acc = f04ComposeLabelValue(
			f07PilotsLiveText(_state->_txtAccuracyBonus),
			f07PilotsLiveText(_state->_txtAccuracyBonusValue));
		const std::string dodge = f04ComposeLabelValue(
			f07PilotsLiveText(_state->_txtDodgeBonus),
			f07PilotsLiveText(_state->_txtDodgeBonusValue));
		const std::string approach = f04ComposeLabelValue(
			f07PilotsLiveText(_state->_txtApproachSpeed),
			f07PilotsLiveText(_state->_txtApproachSpeedValue));
		std::size_t assigned = 0;
		int required = 0;
		craftPilotsSeats(*_state, assigned, required);
		model.detail.titleText = acc;
		if (required > 0)
			model.detail.subtitleText = std::to_string(assigned) + " / " + std::to_string(required);
		const char* const metricIds[3] = {"accuracy", "dodge", "approach"};
		const std::string metricTexts[3] = {acc, dodge, approach};
		for (int i = 0; i < 3; ++i)
		{
			if (metricTexts[i].empty()) continue;
			CalypsoTabbedMetric metric{};
			metric.text = metricTexts[i];
			metric.rect = project(f07CraftPilotsDetailMetricRect(wide, metricIds[i]));
			model.detail.metrics.push_back(metric);
		}
		const std::size_t readoutAssigned =
			_state->_lstPilots ? _state->_lstPilots->getTexts() : 0;
		for (int i = 0; i < CalypsoF07CraftPilotsGen::kDetailActionCount; ++i)
		{
			const auto& generatedAction = CalypsoF07CraftPilotsGen::kDetailActions[i];
			const std::string actionId =
				generatedAction.id ? generatedAction.id : "";
			TextButton* widget = nullptr;
			if (actionId == "clear") widget = _state->_btnRemoveAll;
			if (!widget || !widget->getVisible()
				|| !calypsoF07RemoveAllReadoutVisible(readoutAssigned))
				continue;
			CalypsoTabbedAction clear{};
			clear.widget = widget;
			clear.peer = nullptr;
			clear.text = normalize(widget->getText());
			clear.rect = project(f07CraftPilotsDetailActionRect(wide, generatedAction.id));
			clear.tone = calypsoTabbedToneForContract(
				generatedAction.tone ? generatedAction.tone : "safe");
			clear.restFill = generatedAction.fill;
			clear.restBorder = generatedAction.border;
			clear.textColor = generatedAction.text;
			model.detail.actions.push_back(clear);
		}
	}

	// Footer: Add exactly while the native button is visible (assigned rows
	// below required seats), plus Done. A hidden Add stays unpainted and
	// parked instead of holding a stale slot.
	for (int i = 0; i < CalypsoF07CraftPilotsGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF07CraftPilotsGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		TextButton* widget = nullptr;
		if (actionId == "add") widget = _state->_btnAdd;
		else if (actionId == "cancel") widget = _state->_btnOk;
		if (!widget || !widget->getVisible()) continue;
		CalypsoTabbedAction action{};
		action.widget = widget;
		action.peer = nullptr;
		action.text = normalize(widget->getText());
		action.rect = project(f07CraftPilotsActionRect(wide, generatedAction.id));
		action.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		action.restFill = generatedAction.fill;
		action.restBorder = generatedAction.border;
		action.textColor = generatedAction.text;
		model.actions.push_back(action);
	}

	model.cutCornerPx = CalypsoF07CraftPilotsGen::kCutCornerPx;
	model.panelFillTop = CalypsoF07CraftPilotsGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF07CraftPilotsGen::kPanelFillBottom;
	model.frameColor = CalypsoF07CraftPilotsGen::kFrame;
	model.selectedTabColor = CalypsoF07CraftPilotsGen::kSelectedTab;
	model.dividerColor = CalypsoF07CraftPilotsGen::kDivider;
	model.footerDotColor = CalypsoF07CraftPilotsGen::kFooterDot;
	model.textColor = CalypsoF07CraftPilotsGen::kText;
	model.mutedTextColor = CalypsoF07CraftPilotsGen::kMutedText;
	model.selectionColor = CalypsoF07CraftPilotsGen::kSelection;
	model.scrollTrackColor = CalypsoF07CraftPilotsGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF07CraftPilotsGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	// Standard density (no presentation scale in tabbed contracts).
	model.visualScale = 1.0;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF07CraftPilotsGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF07CraftPilotsGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF07CraftPilotsUi::positionWidgets(CraftPilotsState& state, bool wide)
{
	const auto* generated = CalypsoF07CraftPilotsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstPilots, generated->collectionViewport.x,
		generated->collectionViewport.y, generated->collectionViewport.w,
		generated->collectionViewport.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f07CraftPilotsActionRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Add/Clear live on their generated action slots exactly while visible:
	// hidden natives stay parked (visibility flags and handler bindings are
	// never modified), so no parked action hit area survives the migration.
	if (state._btnAdd && state._btnAdd->getVisible())
		f04ApplyRect(state._btnAdd, f07CraftPilotsActionRect(wide, "add"));
	else
		f04ParkOffscreen(state._btnAdd);
	const std::size_t readoutAssigned =
		state._lstPilots ? state._lstPilots->getTexts() : 0;
	if (state._btnRemoveAll && state._btnRemoveAll->getVisible()
		&& calypsoF07RemoveAllReadoutVisible(readoutAssigned))
		f04ApplyRect(state._btnRemoveAll, f07CraftPilotsDetailActionRect(wide, "clear"));
	else
		f04ParkOffscreen(state._btnRemoveAll);
	// Column headers, the required line, and the bonus facts have no
	// painted equivalent outside the tabbed slots; they stay live readout
	// owners without pointer hit areas.
	f04ParkOffscreen(state._txtFiringAcc);
	f04ParkOffscreen(state._txtReactions);
	f04ParkOffscreen(state._txtBravery);
	f04ParkOffscreen(state._txtPilots);
	f04ParkOffscreen(state._txtRequired);
	f04ParkOffscreen(state._txtAccuracyBonus);
	f04ParkOffscreen(state._txtAccuracyBonusValue);
	f04ParkOffscreen(state._txtDodgeBonus);
	f04ParkOffscreen(state._txtDodgeBonusValue);
	f04ParkOffscreen(state._txtApproachSpeed);
	f04ParkOffscreen(state._txtApproachSpeedValue);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstPilots && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstPilots->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF07CraftPilotsUi::applyGeneratedLayout(CraftPilotsState& state, bool wide)
{
	positionWidgets(state, wide);
}

void CalypsoF07CraftPilotsUi::refreshForInit(CraftPilotsState& state)
{
	// Gate-off leaves the legacy layout untouched: without an HD layout
	// there are no HD slots to seat and parking native chrome would
	// corrupt the vanilla presentation the gate promises to preserve.
	if (!state._hdLayout) return;
	// Approved Remove-All enablement correction (§7.4 quirk): derive the
	// readout from the actual assigned-pilot count instead of the
	// onboard-qualified-vs-seats comparison. Kept isolated here (not in
	// layout) so it lands as its own fix commit; it changes no mutation.
	// Applied BEFORE re-seating below (native updateUI settles assignments,
	// bonuses, and the Add gate in init() AFTER configure()) so the
	// Add/Clear hit areas match the corrected visibility immediately.
	// Add/remove semantics are untouched.
	if (state._lstPilots)
		state._btnRemoveAll->setVisible(calypsoF07RemoveAllReadoutVisible(
			state._lstPilots->getTexts()));
	positionWidgets(state, state._hdWideLayout);
}

void CalypsoF07CraftPilotsUi::configure(CraftPilotsState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F07"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f07HdWideLayout();
	// The native pilot list stays the collection widget (scroll position
	// and selection owner); no adapter-owned inspector is created.
	positionWidgets(state, state._hdWideLayout);
	// First-paint readout correction (native updateUI re-applies its quirk
	// on init; refreshForInit re-corrects after — see above).
	if (state._lstPilots)
		state._btnRemoveAll->setVisible(calypsoF07RemoveAllReadoutVisible(
			state._lstPilots->getTexts()));
	const auto* generated = CalypsoF07CraftPilotsGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 pilots generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._lstPilots, state._window, *generated);
	auto* adapter = new CalypsoF07CraftPilotsUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF07CraftPilotsUi::resize(CraftPilotsState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = f07HdWideLayout();
	state._hdWideLayout = wide;
	positionWidgets(state, wide);
	const auto* generated = CalypsoF07CraftPilotsGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it. The native list owns its rows across relayout, so
	// scroll and selection carry without capture/restore.
	calypsoTabbedSeamList(state._lstPilots, state._window, *generated);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
