#ifdef __EMSCRIPTEN__
#include "CalypsoF05SoldierTransformationUi.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include "../Engine/Font.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/SoldierTransformationState.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleSoldierTransformation.h"
#include "../Savegame/Base.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"
#include "CalypsoF04SelectionListShell.h"
#include "CalypsoF05SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF05TransformationReview.generated.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{

std::string f05ReviewTr(Game* game, const std::string& id)
{
	if (!game || !game->getLanguage() || id.empty())
		return std::string();
	return std::string(game->getLanguage()->getString(id));
}

void f05PushReviewText(const Text* widget, std::vector<std::string>& texts, std::vector<char>& enabled)
{
	if (!widget || !widget->getVisible())
		return;
	const std::string text = CommandCenter::calypsoHdNormalizeTtfDisplayText(widget->getText());
	if (text.empty())
		return;
	texts.push_back(text);
	enabled.push_back(1);
}

std::string f05JoinCells(const std::vector<std::string>& cells)
{
	std::string out;
	for (const auto& cell : cells)
	{
		if (cell.empty())
			continue;
		if (!out.empty())
			out += " ";
		out += cell;
	}
	return out;
}

} // namespace

CalypsoF05SoldierTransformationUi::~CalypsoF05SoldierTransformationUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF05SoldierTransformationUi::topState() const
{
	return _state;
}

void CalypsoF05SoldierTransformationUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_btnCancel);
	suppression.add(_state->_btnLeftArrow);
	suppression.add(_state->_btnRightArrow);
	suppression.add(_state->_btnStart);
	suppression.add(_state->_edtSoldier);
	suppression.add(_state->_txtCost);
	suppression.add(_state->_txtTransferTime);
	suppression.add(_state->_txtRecoveryTime);
	suppression.add(_state->_txtRequiredItems);
	suppression.add(_state->_txtItemNameColumn);
	suppression.add(_state->_txtUnitRequiredColumn);
	suppression.add(_state->_txtUnitAvailableColumn);
	suppression.add(_state->_lstRequiredItems);
	suppression.add(_state->_lstStatChanges);
	suppression.add(_state->_hdInspectorList);
}

void CalypsoF05SoldierTransformationUi::refreshReviewRows(SoldierTransformationState& state)
{
	TextList* list = state._hdInspectorList;
	if (!list)
		return;
	std::vector<std::string> texts;
	std::vector<char> enabled;
	Game* game = state._game;
	RuleSoldierTransformation* rule = state._transformationRule;
	Soldier* soldier = state._sourceSoldier;
	Base* base = state._base;
	if (game && game->getMod() && game->getSavedGame() && rule && soldier && base)
	{
		// Identity first: the editable soldier name (the name editor itself
		// stays parked per the F04 profile precedent; the live text is shown).
		if (state._edtSoldier)
		{
			const std::string name = CommandCenter::calypsoHdNormalizeTtfDisplayText(
				state._edtSoldier->getText());
			if (!name.empty())
			{
				texts.push_back(name);
				enabled.push_back(1);
			}
		}
		// Irreversibility framing: funds/items deduct immediately on commit,
		// no undo. USA-ASCII chrome keys (bitmap-path safe).
		{
			const std::string irreversible = f05ReviewTr(game, "STR_CAL_F05_IRREVERSIBLE");
			if (!irreversible.empty())
			{
				texts.push_back(irreversible);
				enabled.push_back(1);
			}
			const std::string eventMayFire = f05ReviewTr(game, "STR_CAL_F05_EVENT_MAY_FIRE");
			if (!eventMayFire.empty())
			{
				texts.push_back(eventMayFire);
				enabled.push_back(1);
			}
		}
		// Cost + funds-after (read-only recompute, same helper as native).
		if (state._txtCost)
		{
			const int funds = game->getSavedGame()->getFunds();
			const int fundsAfter = funds - rule->getCost();
			std::string cost = CommandCenter::calypsoHdNormalizeTtfDisplayText(
				state._txtCost->getText());
			cost += " -> " + Unicode::formatFunding(fundsAfter);
			texts.push_back(cost);
			enabled.push_back(funds >= rule->getCost() ? 1 : 0);
		}
		// Transfer/recovery times (native hides recovery for producedItem
		// retirements; visibility is read, never recomputed).
		f05PushReviewText(state._txtTransferTime, texts, enabled);
		f05PushReviewText(state._txtRecoveryTime, texts, enabled);
		// Quarters result (read-only recompute of the native Start gate).
		{
			const bool needsQuarters = rule->isCreatingClone()
				|| (rule->isAllowingDeadSoldiers() && soldier->getDeath());
			std::ostringstream quarters;
			quarters << f05ReviewTr(game, "STR_LIVING_QUARTERS_PLURAL")
				<< ": " << base->getUsedQuarters() << "/" << base->getAvailableQuarters();
			texts.push_back(quarters.str());
			enabled.push_back(!(needsQuarters && base->getAvailableQuarters() <= base->getUsedQuarters()) ? 1 : 0);
		}
		// Required items + available quantities in rule order (read-only).
		ItemContainer* storage = base->getStorageItems();
		for (const auto& required : rule->getRequiredItems())
		{
			const RuleItem* itemRule = game->getMod()->getItem(required.first);
			std::ostringstream available;
			bool satisfied = false;
			if (itemRule)
			{
				const int have = storage->getItem(itemRule);
				available << have;
				satisfied = have >= required.second;
			}
			std::ostringstream requiredText;
			requiredText << required.second;
			texts.push_back(CommandCenter::calypsoHdNormalizeTtfDisplayText(
				f04ComposeRowText({f05ReviewTr(game, required.first), requiredText.str(), available.str()})));
			enabled.push_back(satisfied || !itemRule ? 1 : 0);
		}
		// Produced-item retirement names both the lost person and the
		// produced transfer item (dynamic, ruleset-driven).
		if (!Mod::isEmptyRuleName(rule->getProducedItem()) && soldier)
		{
			texts.push_back(CommandCenter::calypsoHdNormalizeTtfDisplayText(
				f04ComposeRowText({soldier->getName(), "->", f05ReviewTr(game, rule->getProducedItem())})));
			enabled.push_back(1);
		}
		// Produced soldier type for cloning projects (dynamic name).
		if (!Mod::isEmptyRuleName(rule->getProducedSoldierType()))
		{
			const std::string produced = f05ReviewTr(game, rule->getProducedSoldierType());
			if (!produced.empty())
			{
				texts.push_back(CommandCenter::calypsoHdNormalizeTtfDisplayText(produced));
				enabled.push_back(1);
			}
		}
		// All stat changes verbatim (mana columns, bonus rows, min/max,
		// "?" rerolls preserved exactly as the native matrix shows them).
		if (state._lstStatChanges)
		{
			const auto& matrix = state._lstStatChanges->getCellTextsSnapshot();
			for (const auto& cells : matrix)
			{
				if (cells.empty() || !cells[0])
					continue;
				std::vector<std::string> parts;
				parts.reserve(cells.size());
				for (const Text* cell : cells)
					parts.push_back(cell ? cell->getText() : std::string());
				const std::string row = CommandCenter::calypsoHdNormalizeTtfDisplayText(
					f05JoinCells(parts));
				if (row.empty())
					continue;
				texts.push_back(row);
				enabled.push_back(1);
			}
		}
	}
	list->clearList();
	for (const auto& text : texts)
		list->addRow(1, text.c_str());
	list->scrollTo(0);
	if (state._hdAdapter)
		state._hdAdapter->_rowEnabled = enabled;
}

void CalypsoF05SoldierTransformationUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The permanent review is a registered HD route: missing prerequisites
	// or a missing generated layout fails the route instead of silently
	// skipping to an empty collection (the overlay would fail it closed
	// anyway). No silent commit: Start only commits through the explicit
	// two-press confirm gate; the commit handler itself is unchanged.
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 review prerequisites are unavailable");
	if (!_state->_transformationRule || !_state->_sourceSoldier)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 review transformation is unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f05TransformationReviewLayout(wide);

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
	model.familyId = CalypsoF05TransformationReviewGen::kFamilyId;
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
	// No native title Text exists; the painted title reads the live
	// transformation rule name, so no widget claim applies here.
	model.titleWidget = nullptr;
	model.listWidget = _state->_hdInspectorList;
	model.titleText = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		f05ReviewTr(_state->_game, _state->_transformationRule->getName()));
	model.protocolText = CalypsoF05TransformationReviewGen::kProtocol;
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(generated->minThumbHeight * uiScale));
	// Shared native input geometry in the same logical space, read exactly
	// once per frame: the painter uses these rects instead of its own formula.
	if (_state->_hdInspectorList && _state->_hdInspectorList->isCalypsoHdSelectionList())
	{
		const SDL_Rect track = _state->_hdInspectorList->getCalypsoHdTrackRect();
		const SDL_Rect thumb = _state->_hdInspectorList->getCalypsoHdThumbRect();
		model.hasNativeScrollGeometry = track.w > 0 && track.h > 0;
		model.nativeTrack = {track.x, track.y, track.w, track.h};
		model.nativeThumb = {thumb.x, thumb.y, thumb.w, thumb.h};
		model.nativeThumbVisible = thumb.w > 0 && thumb.h > 0;
	}

	const int slotCount = wide
		? CalypsoF05TransformationReviewGen::kRowSlotWideCount
		: CalypsoF05TransformationReviewGen::kRowSlotCompactCount;
	const auto* slots = wide
		? CalypsoF05TransformationReviewGen::kRowSlotsWide
		: CalypsoF05TransformationReviewGen::kRowSlotsCompact;
	for (int i = 0; i < slotCount; ++i)
		model.rowSlots.push_back(project(slots[i]));

	// Inspector rows are rebuilt from live native widgets outside collection
	// (refreshReviewRows on configure/init); the inspector list owns scroll
	// position while the painted window follows it. Shortfall rows paint
	// muted, never silent.
	if (_state->_hdInspectorList)
	{
		const auto& matrix = _state->_hdInspectorList->getCellTextsSnapshot();
		for (size_t row = 0; row < matrix.size(); ++row)
		{
			const auto& cells = matrix[row];
			if (cells.empty() || !cells[0])
				continue;
			CalypsoSelectionListRow entry{};
			entry.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
				cells[0]->getText());
			entry.enabled = row < _rowEnabled.size() ? _rowEnabled[row] != 0 : true;
			model.rows.push_back(entry);
		}
		model.scrollOffset = _state->_hdInspectorList->getScroll();
	}
	model.hasSelection = false;
	model.selectedRow = 0;

	model.cutCornerPx = CalypsoF05TransformationReviewGen::kCutCornerPx;
	model.protocolTextInsetPx = CalypsoF05TransformationReviewGen::kProtocolTextInsetPx;
	model.panelFillTop = CalypsoF05TransformationReviewGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF05TransformationReviewGen::kPanelFillBottom;
	model.frameColor = CalypsoF05TransformationReviewGen::kFrame;
	model.protocolColor = CalypsoF05TransformationReviewGen::kProtocolText;
	model.dividerColor = CalypsoF05TransformationReviewGen::kDivider;
	model.footerDotColor = CalypsoF05TransformationReviewGen::kFooterDot;
	model.textColor = CalypsoF05TransformationReviewGen::kText;
	model.mutedTextColor = CalypsoF05TransformationReviewGen::kMutedText;
	model.selectionColor = CalypsoF05TransformationReviewGen::kSelection;
	model.scrollTrackColor = CalypsoF05TransformationReviewGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF05TransformationReviewGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	model.visualScale = CalypsoF05TransformationReviewGen::kPresentationScale;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF05TransformationReviewGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF05TransformationReviewGen::kMotionScaleFrom;

	// The single shared action slot hosts Start in its danger styling: the
	// native transformation-named label normally, the explicit confirm label
	// once the gate arms. Cancel stays keyboard-reachable (keyCancel); the
	// template carries exactly one painted action.
	const auto& generatedButton = CalypsoF05TransformationReviewGen::kButtons[0];
	TextButton* widget = _state->_btnStart;
	model.cancel.widget = widget;
	model.cancel.peer = nullptr;
	model.cancel.text = CommandCenter::calypsoHdNormalizeTtfDisplayText(
		widget ? widget->getText() : std::string());
	model.cancel.rect = project(f05TransformationReviewButtonRect(wide, generatedButton.id));
	model.cancel.tone = std::string(generatedButton.tone) == "danger"
		? CalypsoActionTone::Destructive : CalypsoActionTone::Safe;
	model.cancel.restFill = generatedButton.fill;
	model.cancel.restBorder = generatedButton.border;
	model.cancel.textColor = generatedButton.text;

	calypsoCollectSelectionList(builder, model, _motion);
}

void CalypsoF05SoldierTransformationUi::applyGeneratedLayout(SoldierTransformationState& state, bool wide)
{
	const auto* generated = CalypsoF05TransformationReviewGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	if (state._hdInspectorList)
	{
		f04ApplyRect(state._hdInspectorList, generated->list.x, generated->list.y,
			generated->list.w, generated->list.h);
		// Column geometry follows the HD design list size BEFORE any row is
		// added (statistics rebase precedent); later layouts keep rows.
		if (state._hdInspectorList->getRowsDoNotUse() == 0)
		{
			state._hdInspectorList->rebaseNativeSize(generated->list.w, generated->list.h);
			state._hdInspectorList->setColumns(1, generated->list.w);
		}
	}
	{
		const CalypsoLogicalRect touch = f04TouchRect(f05TransformationReviewButtonRect(wide, "start"));
		f04ApplyRect(state._btnStart, touch);
	}
	// Every remaining native control stays a live input/behavior owner
	// (handlers, gates, and keyboard paths untouched) without painted
	// controls or pointer hit areas. The name editor follows the F04 profile
	// precedent (live text shown, parked field); personnel-cycle arrows keep
	// their keyboard paths; Cancel keeps keyCancel.
	f04ParkOffscreen(state._btnCancel);
	f04ParkOffscreen(state._btnLeftArrow);
	f04ParkOffscreen(state._btnRightArrow);
	f04ParkOffscreen(state._edtSoldier);
	f04ParkOffscreen(state._txtCost);
	f04ParkOffscreen(state._txtTransferTime);
	f04ParkOffscreen(state._txtRecoveryTime);
	f04ParkOffscreen(state._txtRequiredItems);
	f04ParkOffscreen(state._txtItemNameColumn);
	f04ParkOffscreen(state._txtUnitRequiredColumn);
	f04ParkOffscreen(state._txtUnitAvailableColumn);
	f04ParkOffscreen(state._lstRequiredItems);
	f04ParkOffscreen(state._lstStatChanges);
	if (state._hdInspectorList && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._hdInspectorList->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF05SoldierTransformationUi::configure(SoldierTransformationState& state)
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
	// Adapter-owned inspector list: carries the painted review rows and owns
	// scroll position; it binds no click handler and commits nothing.
	if (!state._hdInspectorList)
	{
		state._hdInspectorList = new TextList(8, 8, -4096, -4096);
		state._hdInspectorList->setSelectable(false);
		state._hdInspectorList->setScrolling(true, 0);
		state.add(state._hdInspectorList, "list", "soldierTransformation");
	}
	applyGeneratedLayout(state, state._hdWideLayout);
	refreshReviewRows(state);
	const auto* generated = CalypsoF05TransformationReviewGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F05 review generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Native HD selection-list seam AFTER scaling: shares the inset track and
	// native scroll count/reveal with the painted slots.
	f05SeamSelectionList(state._hdInspectorList, state._window, *generated);
	// Explicit confirm: the gate replaces the immediate-commit Start bindings
	// AFTER the native ctor binds them (configure runs last). Cancel keeps
	// its native safe-pop binding and keyCancel path untouched.
	state._btnStart->onMouseClick((ActionHandler)&SoldierTransformationState::hdStartClickGate);
	state._btnStart->onKeyboardPress((ActionHandler)&SoldierTransformationState::hdStartClickGate, Options::keyOk);
	auto* adapter = new CalypsoF05SoldierTransformationUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF05SoldierTransformationUi::resize(SoldierTransformationState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = f05HdWideLayout();
	state._hdWideLayout = wide;
	applyGeneratedLayout(state, wide);
	const auto* generated = CalypsoF05TransformationReviewGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it. Inspector rows are stable per review instance (values
	// refresh through initTransformationData); selection never applies.
	f05SeamSelectionList(state._hdInspectorList, state._window, *generated);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
