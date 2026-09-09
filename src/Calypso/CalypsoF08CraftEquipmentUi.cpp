#ifdef __EMSCRIPTEN__
#include "CalypsoF08CraftEquipmentUi.h"
#include <algorithm>
#include <cmath>
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
#include "../Interface/Window.h"
#include "../Basescape/CraftEquipmentState.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoF08SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF08CraftEquipment.generated.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

// Tabbed contracts carry only the template style words, so selection,
// scroll, and footer-dot chrome reuse the approved cross-family values
// (identical to the retired selection-list contracts).
constexpr std::uint32_t kF08ChromeFooterDot = 0x74FFB01Fu;
constexpr std::uint32_t kF08ChromeScrollTrack = 0x061B1CD6u;
constexpr std::uint32_t kF08ChromeScrollThumb = 0x74FFB099u;

} // namespace

// Live native rows only: item plus stores plus on-board quantity cells
// composed verbatim in native visible-item order; stable ids are the
// ruleset item types (never translated text). Every row paints enabled:
// transfer impossibility is already the native silent no-op (there is no
// native disabled readout to mirror), and the transfer error handoffs must
// stay reachable through the unchanged arrow handlers.
CalypsoF08CraftEquipmentUi::ItemRows CalypsoF08CraftEquipmentUi::itemRows(const CraftEquipmentState& state)
{
	ItemRows out{};
	const TextList* list = state._lstEquipment;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size() + 3);
	for (std::size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		if (row < state._items.size())
			out.ids.push_back(state._items[row]);
		else
			out.ids.push_back(texts[0]);
		CalypsoTabbedRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF08CraftEquipmentUi::~CalypsoF08CraftEquipmentUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF08CraftEquipmentUi::topState() const
{
	return _state;
}

void CalypsoF08CraftEquipmentUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	// Parked controls (filter combo, quick search, Clear/Inventory buttons,
	// headers, scope texts) stay live input/behavior owners: only their
	// blit is suppressed and only their hit area moves (visibility flags
	// and handler bindings are never modified).
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtItem);
	suppression.add(_state->_txtStores);
	suppression.add(_state->_txtAvailable);
	suppression.add(_state->_txtUsed);
	suppression.add(_state->_txtCrew);
	suppression.add(_state->_lstEquipment);
	suppression.add(_state->_cbxFilterBy);
	suppression.add(_state->_btnQuickSearch);
	suppression.add(_state->_btnClear);
	suppression.add(_state->_btnInventory);
	suppression.add(_state->_btnOk);
}

void CalypsoF08CraftEquipmentUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The equipment screen is a registered HD route: missing prerequisites
	// or a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed
	// anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 equipment prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f08CraftEquipmentLayout(wide);

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
	model.familyId = CalypsoF08CraftEquipmentGen::kFamilyId;
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
	model.listWidget = _state->_lstEquipment;
	model.titleText = normalize(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstEquipment && _state->_lstEquipment->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstEquipment->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstEquipment->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	for (int i = 0; i < generated->visibleRows; ++i)
		model.rowSlots.push_back(project(CalypsoF08CraftEquipmentGen::kRowSlots[wide ? 0 : 1][i].rect));

	// Section tab: the equipment screen is a leaf route with no native tab
	// controls, so only the current equipment tab paints (selected,
	// display-only) with its localized section name.
	{
		CalypsoTabbedTab equipment{};
		equipment.id = "equipment";
		equipment.label = normalize(_state->tr("STR_CAL_F08_TITLE_CRAFT_EQUIPMENT"));
		equipment.selected = true;
		equipment.enabled = true;
		equipment.widget = nullptr;
		equipment.rect = project(f08CraftEquipmentTabRect(wide, "equipment"));
		if (!equipment.label.empty()) model.tabs.push_back(equipment);
	}

	// Summary: the live aboard counter (craft-loaded items maintained by
	// the native list build). Base-wide stores have no exact native owner
	// and stay unpainted rather than misleading.
	{
		CalypsoTabbedSummaryField aboard{};
		aboard.text = std::to_string(_state->_totalItems);
		aboard.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftEquipmentGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF08CraftEquipmentGen::kSummaryCount, "aboard"));
		model.summary.push_back(aboard);
	}

	// Filter control: the native combobox stays the behavior/input owner
	// on its generated slot (handler, ruleset categories, and research
	// gates untouched). The painted text mirrors the combo's live display
	// (translated exactly like the native dropdown options).
	for (int i = 0; i < CalypsoF08CraftEquipmentGen::kControlCount; ++i)
	{
		const auto& generatedControl = CalypsoF08CraftEquipmentGen::kControls[i];
		const std::string controlId =
			generatedControl.id ? generatedControl.id : "";
		Surface* widget = nullptr;
		std::string text;
		if (controlId == "filter" && _state->_cbxFilterBy)
		{
			widget = _state->_cbxFilterBy;
			const size_t sel = _state->_cbxFilterBy->getSelected();
			if (sel < _state->_categoryStrings.size())
				text = normalize(_state->tr(_state->_categoryStrings[sel]));
		}
		if (!widget) continue;
		CalypsoTabbedControl control{};
		control.id = controlId;
		control.kind = generatedControl.kind ? generatedControl.kind : "";
		control.text = text;
		control.widget = widget;
		control.rect = project(f08CraftEquipmentControlRect(wide, generatedControl.id));
		if (!control.text.empty()) model.controls.push_back(control);
	}

	// Toolbar: Inventory binds its gated native button; Load/Save resolve
	// to no native widget (keyboard F5/F9 paths preserved) and stay
	// unpainted instead of dead-end controls.
	for (int i = 0; i < CalypsoF08CraftEquipmentGen::kToolbarCount; ++i)
	{
		const auto& generatedAction = CalypsoF08CraftEquipmentGen::kToolbar[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		TextButton* widget = nullptr;
		if (actionId == "inventory") widget = _state->_btnInventory;
		if (!widget || !widget->getVisible()) continue;
		CalypsoTabbedAction toolbar{};
		toolbar.widget = widget;
		toolbar.peer = nullptr;
		toolbar.text = normalize(widget->getText());
		toolbar.rect = project(f08CraftEquipmentToolbarRect(wide, generatedAction.id));
		toolbar.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		toolbar.restFill = generatedAction.fill;
		toolbar.restBorder = generatedAction.border;
		toolbar.textColor = generatedAction.text;
		model.toolbar.push_back(toolbar);
	}

	// Collection columns from the live native header texts. The native
	// list carries three quantity columns (item/stores/aboard); only item
	// and stores own header texts, and the composed rows below carry every
	// native cell verbatim.
	for (int i = 0; i < CalypsoF08CraftEquipmentGen::kColumnCount; ++i)
	{
		const std::string columnId = CalypsoF08CraftEquipmentGen::kColumnIds[i]
			? CalypsoF08CraftEquipmentGen::kColumnIds[i] : "";
		const Text* header = nullptr;
		if (columnId == "item") header = _state->_txtItem;
		else if (columnId == "stores") header = _state->_txtStores;
		CalypsoTabbedColumn column{};
		column.label = header ? normalize(header->getText()) : std::string();
		column.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftEquipmentGen::kColumnHeaders[wide ? 0 : 1],
			CalypsoF08CraftEquipmentGen::kColumnCount, columnId.c_str()));
		if (!column.label.empty()) model.columns.push_back(column);
	}

	// Native population/order: every visible item row in list order. The
	// native list stays the behavior/input owner (arrows, hold, wheel,
	// article, filter, search).
	const ItemRows bound = itemRows(*_state);
	model.rows = bound.rows;
	const std::size_t items = bound.ids.size();
	if (_state->_lstEquipment)
	{
		model.scrollOffset = _state->_lstEquipment->getScroll();
		const unsigned int selected = _state->_lstEquipment->getSelectedRow();
		model.hasSelection = selected < items;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	// Footer: the quick-search editor sits on its generated slot with its
	// live text mirrored in paint (an empty field paints nothing, exactly
	// like any empty text input); Clear paints exactly while its
	// newBattle-only native button is visible; plus Done. Taps on the
	// search slot reach the live editor beneath, so no parked search hit
	// area survives.
	for (int i = 0; i < CalypsoF08CraftEquipmentGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF08CraftEquipmentGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		const CalypsoLogicalRect slot =
			project(f08CraftEquipmentActionRect(wide, generatedAction.id));
		const CalypsoActionTone tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		if (actionId == "search")
		{
			if (!_state->_btnQuickSearch || !_state->_btnQuickSearch->getVisible())
				continue;
			// TextEdit is not a TextButton: the model carries no widget
			// (Rest state), while the live editor seated on the same slot
			// below owns taps, focus, Return, and the visibility option.
			CalypsoTabbedAction search{};
			search.widget = nullptr;
			search.peer = nullptr;
			search.text = normalize(_state->_btnQuickSearch->getText());
			search.rect = slot;
			search.tone = tone;
			search.restFill = generatedAction.fill;
			search.restBorder = generatedAction.border;
			search.textColor = generatedAction.text;
			model.actions.push_back(search);
			continue;
		}
		TextButton* widget = nullptr;
		if (actionId == "clear") widget = _state->_btnClear;
		else if (actionId == "cancel") widget = _state->_btnOk;
		if (!widget || !widget->getVisible()) continue;
		CalypsoTabbedAction action{};
		action.widget = widget;
		action.peer = nullptr;
		action.text = normalize(widget->getText());
		action.rect = slot;
		action.tone = tone;
		action.restFill = generatedAction.fill;
		action.restBorder = generatedAction.border;
		action.textColor = generatedAction.text;
		model.actions.push_back(action);
	}

	model.cutCornerPx = CalypsoF08CraftEquipmentGen::kCutCornerPx;
	model.panelFillTop = CalypsoF08CraftEquipmentGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF08CraftEquipmentGen::kPanelFillBottom;
	model.frameColor = CalypsoF08CraftEquipmentGen::kFrame;
	model.selectedTabColor = CalypsoF08CraftEquipmentGen::kSelectedTab;
	model.dividerColor = CalypsoF08CraftEquipmentGen::kDivider;
	// The tabbed contract carries only the template style words: selection
	// reuses the identical selected-tab word, and scroll/footer-dot chrome
	// keeps the approved cross-family values (visually identical to the
	// retired selection-list contracts).
	model.footerDotColor = kF08ChromeFooterDot;
	model.textColor = CalypsoF08CraftEquipmentGen::kText;
	model.mutedTextColor = CalypsoF08CraftEquipmentGen::kMutedText;
	model.selectionColor = CalypsoF08CraftEquipmentGen::kSelectedTab;
	model.scrollTrackColor = kF08ChromeScrollTrack;
	model.scrollThumbColor = kF08ChromeScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	// Standard density (no presentation scale in tabbed contracts).
	model.visualScale = 1.0;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF08CraftEquipmentGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF08CraftEquipmentGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF08CraftEquipmentUi::applyGeneratedLayout(CraftEquipmentState& state, bool wide)
{
	const auto* generated = CalypsoF08CraftEquipmentGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstEquipment, generated->collectionViewport.x,
		generated->collectionViewport.y, generated->collectionViewport.w,
		generated->collectionViewport.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f08CraftEquipmentActionRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Column headers and the scope/crew facts have no painted equivalent
	// outside the tabbed slots; the item rows carry every native cell
	// verbatim.
	f04ParkOffscreen(state._txtItem);
	f04ParkOffscreen(state._txtStores);
	f04ParkOffscreen(state._txtAvailable);
	f04ParkOffscreen(state._txtUsed);
	f04ParkOffscreen(state._txtCrew);
	// The category filter keeps its native handler, ruleset categories, and
	// research gates on its generated control slot.
	f04ApplyRect(state._cbxFilterBy, f08CraftEquipmentControlRect(wide, "filter"));
	// The quick search editor keeps its native handlers, search apply, and
	// visibility option on its generated footer slot; a hidden editor stays
	// parked (flag untouched) instead of a stale slot.
	if (state._btnQuickSearch && state._btnQuickSearch->getVisible())
		f04ApplyRect(state._btnQuickSearch, f08CraftEquipmentActionRect(wide, "search"));
	else
		f04ParkOffscreen(state._btnQuickSearch);
	// Clear and Inventory keep their capability visibility, handlers, and
	// keyboard paths on their own generated slots; hidden natives stay
	// parked (flags untouched) instead of stale slots.
	if (state._btnClear && state._btnClear->getVisible())
		f04ApplyRect(state._btnClear, f08CraftEquipmentActionRect(wide, "clear"));
	else
		f04ParkOffscreen(state._btnClear);
	if (state._btnInventory && state._btnInventory->getVisible())
		f04ApplyRect(state._btnInventory, f08CraftEquipmentToolbarRect(wide, "inventory"));
	else
		f04ParkOffscreen(state._btnInventory);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstEquipment && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstEquipment->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF08CraftEquipmentUi::configure(CraftEquipmentState& state)
{
	// Gate-off (family id absent) returns to the legacy path untouched.
	if (!state._game || !state._game->getMod()
		|| !state._game->getMod()->isHdUiFamilyEnabled("F08"))
	{
		state._hdLayout = false;
		return;
	}
	state._hdLayout = true;
	state._hdWideLayout = f08HdWideLayout();
	applyGeneratedLayout(state, state._hdWideLayout);
	const auto* generated = CalypsoF08CraftEquipmentGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 equipment generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._lstEquipment, state._window, *generated);
	auto* adapter = new CalypsoF08CraftEquipmentUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF08CraftEquipmentUi::resize(CraftEquipmentState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable item type BEFORE re-layout (pitfall
	// 3), then restore-or-clamp it into the relaid-out rows (never row
	// numbers).
	const ItemRows before = itemRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstEquipment, before.ids);
	const bool wide = f08HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF08CraftEquipmentGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	calypsoTabbedSeamList(state._lstEquipment, state._window, *generated);
	const ItemRows after = itemRows(state);
	f04RestoreListSelection(state._lstEquipment, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
