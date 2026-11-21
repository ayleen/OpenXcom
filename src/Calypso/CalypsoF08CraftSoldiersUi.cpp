#ifdef __EMSCRIPTEN__
#include "CalypsoF08CraftSoldiersUi.h"
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
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/CraftSoldiersState.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Soldier.h"
#include "CalypsoF06SelectionListShell.h"
#include "CalypsoF08SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF08CraftSoldiers.generated.h"

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

// Live native rows only: name/rank/craft cells (plus the dynamic stat cell
// in combo-sort mode) composed verbatim in native base-diver order; stable
// ids are soldier names (proper nouns, never translated). The enabled flag
// is a readout only: the native list stays the behavior/input owner and its
// click still no-ops exactly where it does today (capacity-full additions,
// STR_OUT-craft rows). Assignment removal always works, so assigned rows
// always paint enabled.
CalypsoF08CraftSoldiersUi::SoldierRows CalypsoF08CraftSoldiersUi::soldierRows(const CraftSoldiersState& state)
{
	SoldierRows out{};
	const TextList* list = state._lstSoldiers;
	if (!list) return out;
	const auto& matrix = list->getCellTextsSnapshot();
	const std::vector<Soldier*>* soldiers = state._base ? state._base->getSoldiers() : nullptr;
	const std::vector<Craft*>* crafts = state._base ? state._base->getCrafts() : nullptr;
	Craft* craft = (crafts && state._craft < crafts->size()) ? (*crafts)[state._craft] : nullptr;
	const int spaceAvailable = craft ? craft->getSpaceAvailable() : 1;
	out.ids.reserve(matrix.size());
	out.rows.reserve(matrix.size() + 2);
	for (std::size_t row = 0; row < matrix.size(); ++row)
	{
		const auto& cells = matrix[row];
		if (cells.empty() || !cells[0]) continue;
		std::vector<std::string> texts;
		texts.reserve(cells.size());
		for (const Text* cell : cells)
			texts.push_back(cell ? cell->getText() : std::string());
		out.ids.push_back(texts[0]);
		CalypsoTabbedRow entry{};
		entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
			f04ComposeRowText(texts));
		entry.enabled = true;
		if (soldiers && row < soldiers->size() && (*soldiers)[row] && craft)
		{
			Soldier* soldier = (*soldiers)[row];
			Craft* assignment = soldier->getCraft();
			if (assignment == craft)
			{
				entry.enabled = true;
			}
			else if (assignment && assignment->getStatus() == "STR_OUT")
			{
				entry.enabled = false;
			}
			else if (spaceAvailable <= 0)
			{
				entry.enabled = false;
			}
		}
		out.rows.push_back(entry);
	}
	return out;
}

CalypsoF08CraftSoldiersUi::~CalypsoF08CraftSoldiersUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF08CraftSoldiersUi::topState() const
{
	return _state;
}

void CalypsoF08CraftSoldiersUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	// Parked controls (_cbxSortBy, _btnPreview, headers, scope texts) stay
	// live input/behavior owners: only their blit is suppressed and only
	// their hit area moves (visibility flags and handler bindings are never
	// modified).
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_txtTitle);
	suppression.add(_state->_txtName);
	suppression.add(_state->_txtRank);
	suppression.add(_state->_txtCraft);
	suppression.add(_state->_txtAvailable);
	suppression.add(_state->_txtUsed);
	suppression.add(_state->_lstSoldiers);
	suppression.add(_state->_cbxSortBy);
	suppression.add(_state->_btnPreview);
	suppression.add(_state->_btnOk);
}

void CalypsoF08CraftSoldiersUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The crew screen is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 crew prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f08CraftSoldiersLayout(wide);

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
	model.familyId = CalypsoF08CraftSoldiersGen::kFamilyId;
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
	model.listWidget = _state->_lstSoldiers;
	model.titleText = normalize(
		_state->_txtTitle ? _state->_txtTitle->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_lstSoldiers && _state->_lstSoldiers->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_lstSoldiers->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_lstSoldiers->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	for (int i = 0; i < generated->visibleRows; ++i)
		model.rowSlots.push_back(project(CalypsoF08CraftSoldiersGen::kRowSlots[wide ? 0 : 1][i].rect));

	// Section tab: the crew screen is a leaf route with no native tab
	// controls, so only the current crew tab paints (selected,
	// display-only) with its localized section name.
	{
		CalypsoTabbedTab crew{};
		crew.id = "crew";
		crew.label = normalize(_state->tr("STR_CAL_F08_TITLE_CRAFT_SOLDIERS"));
		crew.selected = true;
		crew.enabled = true;
		crew.widget = nullptr;
		crew.rect = project(f08CraftSoldiersTabRect(wide, "crew"));
		if (!crew.label.empty()) model.tabs.push_back(crew);
	}

	// Summary: capacity scope verbatim from the live native texts.
	{
		CalypsoTabbedSummaryField capacity{};
		capacity.text = normalize(
			_state->_txtUsed ? _state->_txtUsed->getText() : std::string());
		capacity.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftSoldiersGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF08CraftSoldiersGen::kSummaryCount, "capacity"));
		if (!capacity.text.empty()) model.summary.push_back(capacity);
		CalypsoTabbedSummaryField available{};
		available.text = normalize(
			_state->_txtAvailable ? _state->_txtAvailable->getText() : std::string());
		available.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftSoldiersGen::kSummaryRects[wide ? 0 : 1],
			CalypsoF08CraftSoldiersGen::kSummaryCount, "available"));
		if (!available.text.empty()) model.summary.push_back(available);
	}

	// Sort control: the native combobox stays the behavior/input owner on
	// its generated slot (handler, options, and original-order restore
	// untouched). The selected option text is not readable back from the
	// widget, so the slot paints the combo's own live label; the live
	// options show in the native dropdown on tap.
	for (int i = 0; i < CalypsoF08CraftSoldiersGen::kControlCount; ++i)
	{
		const auto& generatedControl = CalypsoF08CraftSoldiersGen::kControls[i];
		const std::string controlId =
			generatedControl.id ? generatedControl.id : "";
		Surface* widget = nullptr;
		std::string text;
		if (controlId == "sort")
		{
			widget = _state->_cbxSortBy;
			text = normalize(_state->tr("STR_SORT_BY"));
		}
		if (!widget) continue;
		CalypsoTabbedControl control{};
		control.id = controlId;
		control.kind = generatedControl.kind ? generatedControl.kind : "";
		control.text = text;
		control.widget = widget;
		control.rect = project(f08CraftSoldiersControlRect(wide, generatedControl.id));
		if (!control.text.empty()) model.controls.push_back(control);
	}

	// Collection columns from the live native header texts.
	for (int i = 0; i < CalypsoF08CraftSoldiersGen::kColumnCount; ++i)
	{
		const std::string columnId = CalypsoF08CraftSoldiersGen::kColumnIds[i]
			? CalypsoF08CraftSoldiersGen::kColumnIds[i] : "";
		const Text* header = nullptr;
		if (columnId == "diver") header = _state->_txtName;
		else if (columnId == "rank") header = _state->_txtRank;
		else if (columnId == "assignment") header = _state->_txtCraft;
		CalypsoTabbedColumn column{};
		column.label = header ? normalize(header->getText()) : std::string();
		column.rect = project(calypsoTabbedFindRect(
			CalypsoF08CraftSoldiersGen::kColumnHeaders[wide ? 0 : 1],
			CalypsoF08CraftSoldiersGen::kColumnCount, columnId.c_str()));
		if (!column.label.empty()) model.columns.push_back(column);
	}

	// Native population/order: every physical diver row in list order. The
	// native list stays the behavior/input owner (row click toggles
	// assignment or opens the profile, arrows/wheel reorder deployment,
	// sort runs its native handler).
	const SoldierRows bound = soldierRows(*_state);
	model.rows = bound.rows;
	const std::size_t soldiers = bound.ids.size();
	if (_state->_lstSoldiers)
	{
		model.scrollOffset = _state->_lstSoldiers->getScroll();
		const unsigned int selected = _state->_lstSoldiers->getSelectedRow();
		model.hasSelection = selected < soldiers;
		model.selectedRow = model.hasSelection ? selected : 0;
	}

	// Selected-diver detail. Title and subtitle compose the selected row's
	// verbatim name/rank/craft cells; missions/kills metrics are the live
	// native counts (language-neutral, matching the approved slot
	// semantics). The preview action paints exactly while its native
	// visibility gate allows, so no parked preview hit area survives.
	{
		model.detail.present = true;
		model.detail.panel = project(generated->detailPanel);
		model.detail.titleRect = project(CalypsoF08CraftSoldiersGen::kDetailTitleRects[wide ? 0 : 1]);
		model.detail.subtitleRect = project(CalypsoF08CraftSoldiersGen::kDetailSubtitleRects[wide ? 0 : 1]);
		if (_state->_lstSoldiers)
		{
			const auto& matrix = _state->_lstSoldiers->getCellTextsSnapshot();
			const unsigned int row = _state->_lstSoldiers->getSelectedRow();
			if (row < matrix.size() && !matrix[row].empty() && matrix[row][0])
			{
				model.detail.titleText = normalize(matrix[row][0]->getText());
				std::string subtitle;
				if (matrix[row].size() > 1 && matrix[row][1])
					subtitle = normalize(matrix[row][1]->getText());
				if (matrix[row].size() > 2 && matrix[row][2])
				{
					const std::string assignment = normalize(matrix[row][2]->getText());
					if (!assignment.empty())
						subtitle += (subtitle.empty() ? "" : " \xc2\xb7 ") + assignment;
				}
				model.detail.subtitleText = subtitle;
			}
		}
		{
			const std::vector<Soldier*>* divers =
				_state->_base ? _state->_base->getSoldiers() : nullptr;
			const Soldier* selected = nullptr;
			if (divers && _state->_lstSoldiers)
			{
				const unsigned int row = _state->_lstSoldiers->getSelectedRow();
				if (row < divers->size()) selected = (*divers)[row];
			}
			if (selected)
			{
				const char* const metricIds[2] = {"missions", "kills"};
				const std::string metricTexts[2] = {
					std::to_string(selected->getMissions()),
					std::to_string(selected->getKills())};
				for (int i = 0; i < 2; ++i)
				{
					CalypsoTabbedMetric metric{};
					metric.text = metricTexts[i];
					metric.rect = project(f08CraftSoldiersDetailMetricRect(wide, metricIds[i]));
					model.detail.metrics.push_back(metric);
				}
			}
		}
		for (int i = 0; i < CalypsoF08CraftSoldiersGen::kDetailActionCount; ++i)
		{
			const auto& generatedAction = CalypsoF08CraftSoldiersGen::kDetailActions[i];
			const std::string actionId =
				generatedAction.id ? generatedAction.id : "";
			TextButton* widget = nullptr;
			if (actionId == "preview") widget = _state->_btnPreview;
			if (!widget || !widget->getVisible()) continue;
			CalypsoTabbedAction preview{};
			preview.widget = widget;
			preview.peer = nullptr;
			preview.text = normalize(widget->getText());
			preview.rect = project(f08CraftSoldiersDetailActionRect(wide, generatedAction.id));
			preview.tone = calypsoTabbedToneForContract(
				generatedAction.tone ? generatedAction.tone : "safe");
			preview.restFill = generatedAction.fill;
			preview.restBorder = generatedAction.border;
			preview.textColor = generatedAction.text;
			model.detail.actions.push_back(preview);
		}
	}

	// Footer: Done only.
	for (int i = 0; i < CalypsoF08CraftSoldiersGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF08CraftSoldiersGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		TextButton* widget = nullptr;
		if (actionId == "cancel") widget = _state->_btnOk;
		if (!widget) continue;
		CalypsoTabbedAction done{};
		done.widget = widget;
		done.peer = nullptr;
		done.text = normalize(widget->getText());
		done.rect = project(f08CraftSoldiersActionRect(wide, generatedAction.id));
		done.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		done.restFill = generatedAction.fill;
		done.restBorder = generatedAction.border;
		done.textColor = generatedAction.text;
		model.actions.push_back(done);
	}

	model.cutCornerPx = CalypsoF08CraftSoldiersGen::kCutCornerPx;
	model.panelFillTop = CalypsoF08CraftSoldiersGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF08CraftSoldiersGen::kPanelFillBottom;
	model.frameColor = CalypsoF08CraftSoldiersGen::kFrame;
	model.selectedTabColor = CalypsoF08CraftSoldiersGen::kSelectedTab;
	model.dividerColor = CalypsoF08CraftSoldiersGen::kDivider;
	// The tabbed contract carries only the template style words: selection
	// reuses the identical selected-tab word, and scroll/footer-dot chrome
	// keeps the approved cross-family values (visually identical to the
	// retired selection-list contracts).
	model.footerDotColor = kF08ChromeFooterDot;
	model.textColor = CalypsoF08CraftSoldiersGen::kText;
	model.mutedTextColor = CalypsoF08CraftSoldiersGen::kMutedText;
	model.selectionColor = CalypsoF08CraftSoldiersGen::kSelectedTab;
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
	model.motionDurationMs = CalypsoF08CraftSoldiersGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF08CraftSoldiersGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF08CraftSoldiersUi::applyGeneratedLayout(CraftSoldiersState& state, bool wide)
{
	const auto* generated = CalypsoF08CraftSoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	f04ApplyRect(state._txtTitle, generated->title.x, generated->title.y,
		generated->title.w, generated->title.h);
	f04ApplyRect(state._lstSoldiers, generated->collectionViewport.x,
		generated->collectionViewport.y, generated->collectionViewport.w,
		generated->collectionViewport.h);
	{
		const CalypsoLogicalRect touch = f04TouchRect(f08CraftSoldiersActionRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Column headers and the capacity-scope texts have no painted equivalent
	// outside the tabbed slots; the soldier rows carry every native cell
	// verbatim and the scope facts paint in the summary rail.
	f04ParkOffscreen(state._txtName);
	f04ParkOffscreen(state._txtRank);
	f04ParkOffscreen(state._txtCraft);
	f04ParkOffscreen(state._txtAvailable);
	f04ParkOffscreen(state._txtUsed);
	// The sort combobox stays the live behavior owner (handler, options, and
	// original-order restore untouched) on its generated control slot.
	f04ApplyRect(state._cbxSortBy, f08CraftSoldiersControlRect(wide, "sort"));
	// The Deployment preview button stays the live behavior owner (existing
	// BriefingState(craft) handoff, saved-preview label, and hidePreview
	// visibility gate untouched) on its generated detail slot exactly while
	// visible; a hidden preview stays parked instead of a stale slot. The
	// keyboard-only de-assign actions keep their native owners without
	// painted pointer hit areas.
	if (state._btnPreview && state._btnPreview->getVisible())
		f04ApplyRect(state._btnPreview, f08CraftSoldiersDetailActionRect(wide, "preview"));
	else
		f04ParkOffscreen(state._btnPreview);
	// The generated rowHeight is the full native stride (row box + font
	// spacing). Derive the minimum row box from the live small-font metrics
	// so the projected slots agree with native hit-testing at every scale.
	if (state._lstSoldiers && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._lstSoldiers->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF08CraftSoldiersUi::configure(CraftSoldiersState& state)
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
	const auto* generated = CalypsoF08CraftSoldiersGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F08 crew generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._lstSoldiers, state._window, *generated);
	auto* adapter = new CalypsoF08CraftSoldiersUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF08CraftSoldiersUi::resize(CraftSoldiersState& state)
{
	if (!state._hdLayout) return false;
	// Capture the selection by stable soldier name BEFORE re-layout (pitfall
	// 3), then restore-or-clamp it into the relaid-out rows (never row
	// numbers).
	const SoldierRows before = soldierRows(state);
	const CalypsoF04ListSelection captured = f04CaptureListSelection(
		state._lstSoldiers, before.ids);
	const bool wide = f08HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF08CraftSoldiersGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it.
	calypsoTabbedSeamList(state._lstSoldiers, state._window, *generated);
	const SoldierRows after = soldierRows(state);
	f04RestoreListSelection(state._lstSoldiers, captured, after.ids,
		(size_t)generated->visibleRows);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
