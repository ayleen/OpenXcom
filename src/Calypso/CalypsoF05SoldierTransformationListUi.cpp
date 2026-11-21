#ifdef __EMSCRIPTEN__
#include "CalypsoF05SoldierTransformationListUi.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include "../Engine/Font.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Interface/ComboBox.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldierTransformationListState.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleSoldierTransformation.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF05SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF05TransformationList.generated.h"

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoF05SoldierTransformationListUi::OverviewRows
{
	std::vector<std::string> ids;
	std::vector<CalypsoSelectionListRow> rows;
};

// Live native rows only, in native order: the translated project name plus
// EXACT material capacity and eligible-personnel counts. The native overview
// deliberately caps its display-oriented calculation; the HD rows disclose
// the exact adapter-side figures from the same engine sources (funds, base
// storage, live/dead rosters) without mutating them. Stable ids are the
// transformation rule names (never translated text); external soldier
// types/projects flow through with no fixed Calypso row lists.
CalypsoF05SoldierTransformationListUi::OverviewRows CalypsoF05SoldierTransformationListUi::overviewRows(const SoldierTransformationListState& state)
{
	OverviewRows out{};
	const TextList* list = state._lstTransformations;
	if (!list || !state._game) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size());
	for (size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		if (row >= state._transformationIndices.size()) continue;
		const int native = state._transformationIndices[row];
		if (native < 0 || (size_t)native >= state._availableTransformations.size()) continue;
		const RuleSoldierTransformation* rule = state._availableTransformations[(size_t)native];
		if (!rule) continue;
		out.ids.push_back(rule->getName());
		const CalypsoF05ExactCapacity exact = f05ExactTransformationCapacity(
			rule, state._base, state._game);
		std::ostringstream capacity, eligible;
		if (exact.projectsPossible < 0)
			capacity << '+';
		else
			capacity << exact.projectsPossible;
		eligible << exact.eligibleSoldiers;
		const std::string name = cells[0] ? cells[0]->getText() : std::string();
		CalypsoSelectionListRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText({name, capacity.str(), eligible.str()}));
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF05SoldierTransformationListUi::~CalypsoF05SoldierTransformationListUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF05SoldierTransformationListUi::topState() const
{
	return _state;
}

void CalypsoF05SoldierTransformationListUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtProject);
	suppression.add(_state->_txtNumber);
	suppression.add(_state->_txtSoldierNumber);
	suppression.add(_state->_cbxSoldierType);
	suppression.add(_state->_cbxSoldierStatus);
	suppression.add(_state->_btnQuickSearch);
	suppression.add(_state->_lstTransformations);
	suppression.add(_state->_btnOnlyEligible);
	suppression.add(_state->_btnOK);
}

void CalypsoF05SoldierTransformationListUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The global overview is a registered HD route: missing prerequisites or
	// a missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	// Row activation selects the project in the originating roster combo and
	// pops through the unchanged native handler (it pushes no new state);
	// middle-click opens the project article through the unchanged shortcut.
	// The parent roster-selection contract is retained exactly.
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 overview prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f05TransformationListLayout(wide);

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
	model.familyId = CalypsoF05TransformationListGen::kFamilyId;
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
	model.protocolText = CalypsoF05TransformationListGen::kProtocol;
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
		? CalypsoF05TransformationListGen::kRowSlotWideCount
		: CalypsoF05TransformationListGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF05TransformationListGen::kRowSlotsWide
		: CalypsoF05TransformationListGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Native population/order/filters: every physical row in list order with
	// exact capacity figures; the native list stays the behavior/input owner
	// (combo-select + pop, article shortcut, arrows, wheel).
	const OverviewRows bound = overviewRows(*_state);
	model.rows = bound.rows;
	const std::size_t total = model.rows.size();
	if (_state->_lstTransformations)
	{
		model.scrollOffset = _state->_lstTransformations->getScroll();
		const unsigned int selected = _state->_lstTransformations->getSelectedRow();
		model.hasSelection = selected < total;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	model.cutCornerPx = CalypsoF05TransformationListGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF05TransformationListGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF05TransformationListGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF05TransformationListGen::kPanelFillBottom;
	model.frameColor = CalypsoF05TransformationListGen::kFrame;
	model.protocolColor = CalypsoF05TransformationListGen::kProtocolText;
	model.dividerColor = CalypsoF05TransformationListGen::kDivider;
	model.footerDotColor = CalypsoF05TransformationListGen::kFooterDot;
	model.textColor = CalypsoF05TransformationListGen::kText;
	model.mutedTextColor = CalypsoF05TransformationListGen::kMutedText;
	model.selectionColor = CalypsoF05TransformationListGen::kSelection;
	model.scrollTrackColor = CalypsoF05TransformationListGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF05TransformationListGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF05TransformationListGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF05TransformationListGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF05TransformationListGen::kMotionScaleFrom;

	const auto& generatedButton = CalypsoF05TransformationListGen::kButtons[0];
	TextButton* widget = _state->_btnOK;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f05TransformationListButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF05SoldierTransformationListUi::applyGeneratedLayout(SoldierTransformationListState& state, bool wide)
{
	const auto* generated = CalypsoF05TransformationListGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstTransformations, generated->list.x, generated->list.y,
		generated->list.w, generated->list.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f05TransformationListButtonRect(wide, "cancel"));
		f04ApplyRect(state._btnOK, touch);
	}
	// Project/materials/soldiers headers have no painted equivalent in the
	// shared shell; the composed rows carry name + exact figures verbatim.
	f04ParkOffscreen(state._txtProject);
	f04ParkOffscreen(state._txtNumber);
	f04ParkOffscreen(state._txtSoldierNumber);
	// Filters stay live native input owners (type/status combos, eligible
	// toggle with its persisted option, quick search with its toggle/apply
	// handlers — all untouched) without painted controls or pointer hit
	// areas; keyboard paths behave exactly as before.
	f04ParkOffscreen(state._cbxSoldierType);
	f04ParkOffscreen(state._cbxSoldierStatus);
	f04ParkOffscreen(state._btnOnlyEligible);
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

void CalypsoF05SoldierTransformationListUi::configure(SoldierTransformationListState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F05"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f05HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF05TransformationListGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 overview generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f05SeamSelectionList(state._lstTransformations, state._window, *generated);
	auto* adapter = new CalypsoF05SoldierTransformationListUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF05SoldierTransformationListUi::resize(SoldierTransformationListState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable id BEFORE re-layout (pitfall 3), then
	// restore-or-clamp it into the relaid-out rows (never row numbers).
	const OverviewRows before = overviewRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstTransformations, before.ids);
	const bool wide = f05HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF05TransformationListGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	f05SeamSelectionList(state._lstTransformations, state._window, *generated);
	const OverviewRows after = overviewRows(state);
	f04RestoreListSelection(state._lstTransformations, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
