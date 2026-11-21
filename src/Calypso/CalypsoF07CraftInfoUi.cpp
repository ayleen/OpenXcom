#ifdef __EMSCRIPTEN__
#include "CalypsoF07CraftInfoUi.h"
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
#include "../Interface/TextEdit.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Basescape/CraftInfoState.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/CraftWeapon.h"
#include "CalypsoF07SelectionListShell.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiMetrics.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "Generated/CalypsoF07CraftInfo.generated.h"

namespace OpenXcom
{
namespace Calypso
{

namespace
{


std::string f07JoinNonEmpty(const std::vector<std::string>& parts)
{
	std::string out;
	for (const auto& part : parts)
	{
		if (part.empty()) continue;
		if (!out.empty()) out += " \xc2\xb7 ";
		out += part;
	}
	return out;
}

// Native mount ammo texts carry embedded newlines (ammo/max/rearm lines);
// flatten them into the row separator so the single-line row slot keeps one
// logical line. Verbatim words, presentation-only joining.
std::string f07FlattenMultiline(std::string text)
{
	std::string out;
	out.reserve(text.size());
	for (std::size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == '\n' || text[i] == '\r')
		{
			if (!out.empty() && out.size() >= 3
				&& out.compare(out.size() - 3, 3, " \xc2\xb7 ") != 0)
				out += " \xc2\xb7 ";
			continue;
		}
		out.push_back(text[i]);
	}
	return out;
}

std::string f07ButtonLabel(const TextButton* button)
{
	return CommandCenter::calypsoHdNormalizeTtfDisplayText(
		button ? button->getText() : std::string());
}

std::string f07LiveText(const Text* single)
{
	return CommandCenter::calypsoHdNormalizeTtfDisplayText(
		single ? single->getText() : std::string());
}

} // namespace

// One painted collection row per live weapon mount, in slot order. Tabs,
// summary facts, and footer live on their own generated slots now (no
// flattened action rows); the debug-only New Battle entry and the skin facts
// have no contract slot and stay parked live owners instead of rows.
std::vector<CalypsoF07CraftInfoUi::CraftInfoRow> CalypsoF07CraftInfoUi::craftInfoPlan(const CraftInfoState& state)
{
	std::vector<CraftInfoRow> plan;
	const int mounts = std::min(state._weaponNum, RuleCraft::WeaponMax);
	for (int i = 0; i < mounts; ++i)
	{
		CraftInfoRow row{};
		row.slot = i;
		row.text = f07JoinNonEmpty({
			f07ButtonLabel(state._btnW[i]),
			f07FlattenMultiline(f07LiveText(state._txtWName[i])),
			f07FlattenMultiline(f07LiveText(state._txtWAmmo[i]))});
		// A hidden Change button (fixed weapon slot) leaves a display-only
		// row: the mount facts stay readable, no dead-end is implied.
		row.enabled = state._btnW[i] && state._btnW[i]->getVisible();
		if (!row.text.empty()) plan.push_back(row);
	}
	return plan;
}

CalypsoF07CraftInfoUi::~CalypsoF07CraftInfoUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void* CalypsoF07CraftInfoUi::topState() const
{
	return _state;
}

void CalypsoF07CraftInfoUi::collectLogicalSuppression(
	CalypsoHdLogicalSuppression& suppression) const
{
	// Whole-state suppression (suppressLogicalState, default true) owns the
	// top-state logical UI; list every native surface explicitly so widget
	// suppression also holds during warmup and under a covered child.
	if (!_state) return;
	suppression.add(_state->_window);
	suppression.add(_state->_edtCraft);
	suppression.add(_state->_txtDamage);
	suppression.add(_state->_txtShield);
	suppression.add(_state->_txtFuel);
	suppression.add(_state->_txtSkin);
	const int mounts = std::min(_state->_weaponNum, RuleCraft::WeaponMax);
	for (int i = 0; i < mounts; ++i)
	{
		suppression.add(_state->_btnW[i]);
		suppression.add(_state->_txtWName[i]);
		suppression.add(_state->_txtWAmmo[i]);
		suppression.add(_state->_weapon[i]);
	}
	suppression.add(_state->_sprite);
	suppression.add(_state->_crew);
	suppression.add(_state->_equip);
	suppression.add(_state->_btnOk);
	suppression.add(_state->_btnCrew);
	suppression.add(_state->_btnEquip);
	suppression.add(_state->_btnArmor);
	suppression.add(_state->_btnPilots);
	suppression.add(_state->_btnNewBattle);
	if (_state->_hdInspectorList)
		suppression.add(_state->_hdInspectorList);
}

void CalypsoF07CraftInfoUi::collect(CalypsoHdFrameBuilder& builder) const
{
	// The overview is a registered HD route: missing prerequisites or a
	// missing generated layout fails the route instead of silently skipping
	// to an empty collection (the overlay would fail it closed anyway).
	if (!_state || !_state->_hdLayout || !_state->_game)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 overview prerequisites are unavailable");
	const bool wide = _state->_hdWideLayout;
	const auto* generated = f07CraftInfoLayout(wide);

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
	model.familyId = CalypsoF07CraftInfoGen::kFamilyId;
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
	// No native title Text exists: the live craft-name editor owns the title
	// slot and the painted title mirrors its live text every frame.
	model.titleWidget = nullptr;
	model.listWidget = _state->_hdInspectorList;
	// Editable craft name, verbatim: empty resets to default and Return
	// refreshes through the unchanged native handler.
	model.titleText = normalize(
		_state->_edtCraft ? _state->_edtCraft->getText() : std::string());
	model.rowHeight = generated->rowHeight;
	model.visibleRows = generated->visibleRows;
	model.scrollBarWidth = generated->scrollBarWidth;
	model.minThumbHeight = std::max(1, (int)std::llround(CALYPSO_MIN_TOUCH_TARGET * uiScale));
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

	// Grid mount cards carry no row slots: the contract emits a zero
	// placeholder where list/table contracts carry strides, and tiles
	// below own the full card geometry instead.

	// Section tabs. The overview tab marks the current screen (selected,
	// display-only); the live craft name is its only owner text (a proper
	// noun, never translated). Capability tabs bind their live native
	// buttons exactly while visible (native init hides all four when
	// RuleCraft::getMaxUnitsLimit() == 0; Pilots additionally requires
	// RuleCraft::getPilots() > 0 from the ctor gate), so zero-capacity
	// craft paint the overview tab only. Hidden routes stay closed: no
	// placeholder tab, no dead-end. The weapons section has no native tab
	// route from this screen (mounts are collection rows with their own
	// Change hit areas), so it paints no tab.
	{
		CalypsoTabbedTab overview{};
		overview.id = "overview";
		overview.label = normalize(
			_state->_edtCraft ? _state->_edtCraft->getText() : std::string());
		overview.selected = true;
		overview.enabled = true;
		overview.widget = nullptr;
		overview.rect = project(f07CraftInfoTabRect(wide, "overview"));
		if (!overview.label.empty()) model.tabs.push_back(overview);
	}
	{
		TextButton* const buttons[4] = {_state->_btnCrew, _state->_btnEquip, _state->_btnArmor, _state->_btnPilots};
		const char* const ids[4] = {"crew", "equipment", "armor", "pilots"};
		for (int i = 0; i < 4; ++i)
		{
			TextButton* button = buttons[i];
			if (!button || !button->getVisible()) continue;
			CalypsoTabbedTab tab{};
			tab.id = ids[i];
			tab.label = normalize(button->getText());
			tab.selected = false;
			tab.enabled = true;
			tab.widget = button;
			tab.rect = project(f07CraftInfoTabRect(wide, ids[i]));
			if (!tab.label.empty()) model.tabs.push_back(tab);
		}
	}

	// Summary facts: base and localized status from live craft data, damage
	// and fuel verbatim from their native texts. Shield and skin have no
	// contract slot and stay parked live readout owners.
	{
		const std::string baseName =
			_state->_base ? _state->_base->getName() : std::string();
		std::string statusText;
		if (_state->_craft) statusText = _state->tr(_state->_craft->getStatus());
		CalypsoTabbedSummaryField base{};
		base.text = normalize(baseName);
		base.rect = project(f07CraftInfoSummaryRect(wide, "base"));
		if (!base.text.empty()) model.summary.push_back(base);
		CalypsoTabbedSummaryField status{};
		status.text = normalize(statusText);
		status.rect = project(f07CraftInfoSummaryRect(wide, "status"));
		if (!status.text.empty()) model.summary.push_back(status);
		CalypsoTabbedSummaryField damage{};
		damage.text = f07LiveText(_state->_txtDamage);
		damage.rect = project(f07CraftInfoSummaryRect(wide, "damage"));
		if (!damage.text.empty()) model.summary.push_back(damage);
		CalypsoTabbedSummaryField fuel{};
		fuel.text = f07LiveText(_state->_txtFuel);
		fuel.rect = project(f07CraftInfoSummaryRect(wide, "fuel"));
		if (!fuel.text.empty()) model.summary.push_back(fuel);
	}

	// Mount cards from the live plan in generated tile order. Each tile
	// gets the projected card/label rects, the full live entry text, the
	// native availability verdict, and the mount Change button exactly
	// while visible; a hidden Change button binds the live icon surface
	// instead, so the toggle/article handler keeps an owner, and hidden
	// widgets bind nothing. No fabricated labels or data. Tiles never
	// scroll, so no row slots, selection, or scroll offset apply here.
	std::vector<CraftInfoRow> plan = craftInfoPlan(*_state);
	const int leadSlot = plan.empty() ? -1 : plan[0].slot;
	{
		const auto& tileSlots = CalypsoF07CraftInfoGen::kTileSlots[wide ? 0 : 1];
		const int tileCapacity =
			CalypsoF07CraftInfoGen::kTileSlotCounts[wide ? 0 : 1];
		const int mounts = std::min(_state->_weaponNum, RuleCraft::WeaponMax);
		int tileIdx = 0;
		for (const auto& entry : plan)
		{
			if (entry.slot < 0 || entry.slot >= mounts || tileIdx >= tileCapacity) continue;
			CalypsoTabbedTile tile{};
			tile.rect = project(tileSlots[tileIdx].rect);
			tile.labelRect = project(tileSlots[tileIdx].label);
			tile.label = entry.text;
			tile.enabled = entry.enabled;
			TextButton* change = _state->_btnW[entry.slot];
			if (change && change->getVisible())
				tile.widget = change;
			else
				tile.widget = _state->_weapon[entry.slot];
			model.tiles.push_back(tile);
			++tileIdx;
		}
	}
	model.hasSelection = false;
	model.selectedRow = 0;
	model.scrollOffset = 0;

	// Selected-mount inspector from live native mount data. Title mirrors
	// the live weapon name (including the native disabled "*" mark),
	// subtitle the live slot number, ammo the live loaded/max counts
	// (skipped for mounts without an ammo maximum), and status the live
	// localized craft status. Change binds the lead mount's native Change
	// button exactly while visible. The Reference entry has no native tap
	// owner (article stays middle-click on the row icon plus the keyboard
	// shortcut), so it paints nothing instead of a dead-end control.
	{
		model.detail.present = !plan.empty();
		model.detail.panel = project(generated->detailPanel);
		model.detail.titleRect = project(CalypsoF07CraftInfoGen::kDetailTitleRects[wide ? 0 : 1]);
		model.detail.subtitleRect = project(CalypsoF07CraftInfoGen::kDetailSubtitleRects[wide ? 0 : 1]);
		const int mounts = std::min(_state->_weaponNum, RuleCraft::WeaponMax);
		if (leadSlot >= 0 && leadSlot < mounts)
		{
			model.detail.titleText = f07LiveText(_state->_txtWName[leadSlot]);
			model.detail.subtitleText = f07ButtonLabel(_state->_btnW[leadSlot]);
			const CraftWeapon* mount = nullptr;
			if (_state->_craft && _state->_craft->getWeapons()
				&& (size_t)leadSlot < _state->_craft->getWeapons()->size())
				mount = (*_state->_craft->getWeapons())[leadSlot];
			if (mount && mount->getRules() && mount->getRules()->getAmmoMax() > 0)
			{
				CalypsoTabbedMetric ammo{};
				ammo.text = std::to_string(mount->getAmmo())
					+ " / " + std::to_string(mount->getRules()->getAmmoMax());
				ammo.rect = project(calypsoTabbedFindRect(
					CalypsoF07CraftInfoGen::kDetailMetricRects[wide ? 0 : 1],
					CalypsoF07CraftInfoGen::kDetailMetricCount, "ammo"));
				if (!ammo.text.empty()) model.detail.metrics.push_back(ammo);
			}
			std::string mountStatus;
			if (_state->_craft) mountStatus = _state->tr(_state->_craft->getStatus());
			if (!mountStatus.empty())
			{
				CalypsoTabbedMetric status{};
				status.text = normalize(mountStatus);
				status.rect = project(calypsoTabbedFindRect(
					CalypsoF07CraftInfoGen::kDetailMetricRects[wide ? 0 : 1],
					CalypsoF07CraftInfoGen::kDetailMetricCount, "status"));
				model.detail.metrics.push_back(status);
			}
		}
		for (int i = 0; i < CalypsoF07CraftInfoGen::kDetailActionCount; ++i)
		{
			const auto& generatedAction = CalypsoF07CraftInfoGen::kDetailActions[i];
			const std::string actionId =
				generatedAction.id ? generatedAction.id : "";
			if (actionId != "change") continue;
			TextButton* widget = (leadSlot >= 0 && leadSlot < mounts)
				? _state->_btnW[leadSlot] : nullptr;
			if (!widget || !widget->getVisible()) continue;
			CalypsoTabbedAction change{};
			change.widget = widget;
			change.peer = nullptr;
			change.text = normalize(widget->getText());
			change.rect = project(calypsoTabbedFindRect(
				CalypsoF07CraftInfoGen::kDetailActionRects[wide ? 0 : 1],
				CalypsoF07CraftInfoGen::kDetailActionCount, generatedAction.id));
			change.tone = calypsoTabbedToneForContract(
				generatedAction.tone ? generatedAction.tone : "safe");
			change.restFill = generatedAction.fill;
			change.restBorder = generatedAction.border;
			change.textColor = generatedAction.text;
			model.detail.actions.push_back(change);
		}
	}

	// Footer: Done only. The rename/more fixture entries resolve to no
	// native widget, so they stay unpainted instead of dead-end controls.
	for (int i = 0; i < CalypsoF07CraftInfoGen::kActionCount; ++i)
	{
		const auto& generatedAction = CalypsoF07CraftInfoGen::kActions[i];
		const std::string actionId =
			generatedAction.id ? generatedAction.id : "";
		if (actionId != "cancel") continue;
		TextButton* widget = _state->_btnOk;
		if (!widget) continue;
		CalypsoTabbedAction done{};
		done.widget = widget;
		done.peer = nullptr;
		done.text = normalize(widget->getText());
		done.rect = project(f07CraftInfoActionRect(wide, generatedAction.id));
		done.tone = calypsoTabbedToneForContract(
			generatedAction.tone ? generatedAction.tone : "safe");
		done.restFill = generatedAction.fill;
		done.restBorder = generatedAction.border;
		done.textColor = generatedAction.text;
		model.actions.push_back(done);
	}

	model.cutCornerPx = CalypsoF07CraftInfoGen::kCutCornerPx;
	model.panelFillTop = CalypsoF07CraftInfoGen::kPanelFillTop;
	model.panelFillBottom = CalypsoF07CraftInfoGen::kPanelFillBottom;
	model.frameColor = CalypsoF07CraftInfoGen::kFrame;
	model.selectedTabColor = CalypsoF07CraftInfoGen::kSelectedTab;
	model.dividerColor = CalypsoF07CraftInfoGen::kDivider;
	model.footerDotColor = CalypsoF07CraftInfoGen::kFooterDot;
	model.textColor = CalypsoF07CraftInfoGen::kText;
	model.mutedTextColor = CalypsoF07CraftInfoGen::kMutedText;
	model.selectionColor = CalypsoF07CraftInfoGen::kSelection;
	model.scrollTrackColor = CalypsoF07CraftInfoGen::kScrollTrack;
	model.scrollThumbColor = CalypsoF07CraftInfoGen::kScrollThumb;
	const CalypsoHdPresentationMetrics& metrics =
		CalypsoHdUiOverlay::instance().frozenMetrics();
	model.uiScale = uiScale;
	// Standard density (no presentation scale in tabbed contracts).
	model.visualScale = 1.0;
	model.projectionScaleX = uiScale * metrics.scaleX;
	model.projectionScaleY = uiScale * metrics.scaleY;
	model.titleDesignHeight = generated->title.h;
	model.motionDurationMs = CalypsoF07CraftInfoGen::kMotionDurationMs;
	model.motionScaleFrom = CalypsoF07CraftInfoGen::kMotionScaleFrom;

	calypsoCollectTabbedManagement(builder, model, _motion);
}

void CalypsoF07CraftInfoUi::positionWidgets(CraftInfoState& state, bool wide)
{
	const auto* generated = CalypsoF07CraftInfoGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return;
	f04ApplyRect(state._window, generated->window.x, generated->window.y,
		generated->window.w, generated->window.h);
	// The live name editor sits on the painted title slot: clicks focus the
	// invisible native field, typing/Return/empty-reset run the unchanged
	// handler, and the painted title mirrors the live text every frame.
	{
		const CalypsoLogicalRect touch = f04TouchRect(
			{generated->title.x, generated->title.y, generated->title.w, generated->title.h});
		f04ApplyRect(state._edtCraft, touch);
	}
	if (state._hdInspectorList)
	{
		f04ApplyRect(state._hdInspectorList, generated->collectionViewport.x,
			generated->collectionViewport.y, generated->collectionViewport.w,
			generated->collectionViewport.h);
		// Column geometry follows the HD design list size BEFORE any row is
		// added (statistics rebase precedent); later layouts keep rows.
		if (state._hdInspectorList->getRowsDoNotUse() == 0)
		{
			state._hdInspectorList->rebaseNativeSize(generated->collectionViewport.w,
				generated->collectionViewport.h);
			state._hdInspectorList->setColumns(1, generated->collectionViewport.w);
		}
	}
	{
		const CalypsoLogicalRect touch = f04TouchRect(f07CraftInfoActionRect(wide, "cancel"));
		f04ApplyRect(state._btnOk, touch);
	}
	// Capability tab hit areas sit on their generated tab slots exactly
	// while their native buttons are visible; hidden natives stay parked
	// (visibility flags and handler bindings are never modified), so gated
	// routes stay closed instead of holding a stale slot.
	{
		TextButton* const tabs[4] = {state._btnCrew, state._btnEquip, state._btnArmor, state._btnPilots};
		const char* const ids[4] = {"crew", "equipment", "armor", "pilots"};
		for (int i = 0; i < 4; ++i)
		{
			if (tabs[i] && tabs[i]->getVisible())
				f04ApplyRect(tabs[i], f07CraftInfoTabRect(wide, ids[i]));
			else
				f04ParkOffscreen(tabs[i]);
		}
	}
	// Mount hit areas sit on the matching collection row slots of the same
	// plan that collect() paints, except the lead mount: its Change button
	// moves to the inspector slot below, so its row paints disabled
	// display-only while the icon zone keeps the toggle/article handler
	// live. Visibility flags and handler bindings are never modified:
	// hidden Change buttons stay inert wherever they sit. Park every mount
	// widget first; only planned leading slots reclaim one. Overflow mounts
	// stay parked (visible flag untouched) rather than holding a stale slot
	// that could fire under foreign painted content.
	const std::vector<CraftInfoRow> plan = craftInfoPlan(state);
	const int leadSlot = plan.empty() ? -1 : plan[0].slot;
	const auto& tileSlots = CalypsoF07CraftInfoGen::kTileSlots[wide ? 0 : 1];
	const int tileCapacity = (int)(sizeof(CalypsoF07CraftInfoGen::kTileSlots[wide ? 0 : 1])
		/ sizeof(CalypsoF07CraftInfoGen::kTileSlots[wide ? 0 : 1][0]));
	const int mounts = std::min(state._weaponNum, RuleCraft::WeaponMax);
	for (int i = 0; i < mounts; ++i)
	{
		f04ParkOffscreen(state._btnW[i]);
		f04ParkOffscreen(state._weapon[i]);
	}
	int tileIdx = 0;
	for (const auto& entry : plan)
	{
		if (tileIdx >= tileCapacity) break;
		if (entry.slot < 0 || entry.slot >= mounts) continue;
		const auto& slot = tileSlots[tileIdx].rect;
		const CalypsoLogicalRect card{slot.x, slot.y, slot.w, slot.h};
		{
			// Split card, mirroring the native button/icon separation: the
			// leading zone opens the Change route, the trailing 44px zone
			// owns the weapon icon (click toggles enable, middle-click
			// opens the article through the unchanged handlers). The lead
			// mount keeps only its icon zone: its Change button lives on
			// the inspector slot, matching the detail action collect()
			// paints for that mount.
			const int iconW = std::min(44, card.w / 2);
			if (entry.slot != leadSlot)
				f04ApplyRect(state._btnW[entry.slot],
					CalypsoLogicalRect{card.x, card.y, card.w - iconW, card.h});
			f04ApplyRect(state._weapon[entry.slot],
				CalypsoLogicalRect{card.x + card.w - iconW, card.y, iconW, card.h});
		}
		++tileIdx;
	}
	// Inspector Change hit area: the lead mount's native Change button on
	// its generated slot exactly while visible; otherwise it stays parked
	// with the rest (flag and handler untouched), and the slot paints
	// nothing.
	if (leadSlot >= 0 && leadSlot < mounts && state._btnW[leadSlot]
		&& state._btnW[leadSlot]->getVisible())
		f04ApplyRect(state._btnW[leadSlot], calypsoTabbedFindRect(
			CalypsoF07CraftInfoGen::kDetailActionRects[wide ? 0 : 1],
			CalypsoF07CraftInfoGen::kDetailActionCount, "change"));
	// No contract slot exists for the craft skin toggle or the debug-only
	// New Battle entry: both stay parked live behavior owners (handlers,
	// gates, and visibility flags untouched) without painted controls or
	// pointer hit areas. Pointer activation for the skin toggle is a
	// reported follow-up; the debug battle setup stays reachable through
	// its native visibility gate exactly as before.
	f04ParkOffscreen(state._btnNewBattle);
	f04ParkOffscreen(state._sprite);
	// Every remaining native control stays a live input/behavior owner
	// (handlers, gates, and keyboard paths untouched) without painted
	// controls or pointer hit areas. The craft-article shortcut
	// (keyGeoUfopedia on Done) and the icon middle-clicks keep working for
	// keyboard/mouse users; touch-initiated reference needs a native touch
	// widget that does not exist yet (reported follow-up, no new bindings).
	f04ParkOffscreen(state._txtDamage);
	f04ParkOffscreen(state._txtShield);
	f04ParkOffscreen(state._txtFuel);
	f04ParkOffscreen(state._txtSkin);
	for (int i = 0; i < mounts; ++i)
	{
		f04ParkOffscreen(state._txtWName[i]);
		f04ParkOffscreen(state._txtWAmmo[i]);
	}
	f04ParkOffscreen(state._crew);
	f04ParkOffscreen(state._equip);
	if (state._hdInspectorList && state._game && state._game->getMod())
	{
		const Font* font = state._game->getMod()->getFont("FONT_SMALL");
		const int fontH = font ? font->getHeight() : 0;
		const int spacing = font ? font->getSpacing() : 0;
		state._hdInspectorList->setMinimumRowHeight(
			std::max(fontH, generated->rowHeight - spacing));
	}
}

void CalypsoF07CraftInfoUi::applyGeneratedLayout(CraftInfoState& state, bool wide)
{
	positionWidgets(state, wide);
}

void CalypsoF07CraftInfoUi::refreshForInit(CraftInfoState& state)
{
	// Gate-off leaves the legacy layout untouched: without an HD layout
	// there are no HD slots to seat and parking native chrome would
	// corrupt the vanilla presentation the gate promises to preserve.
	if (!state._hdLayout) return;
	// Native init() settles tab visibility (zero-capacity hide), mount texts,
	// and the fixed-weapon Change gates: recompute hit areas onto the
	// current plan and reset inspector scroll (pitfall 3: the row set just
	// changed shape, so a carried scroll offset cannot restore by id).
	positionWidgets(state, state._hdWideLayout);
	if (state._hdInspectorList)
		state._hdInspectorList->scrollTo(0);
}

void CalypsoF07CraftInfoUi::configure(CraftInfoState& state)
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
	// Adapter-owned inspector list: carries the painted overview rows and
	// owns scroll position; it binds no click handler and commits nothing.
	if (!state._hdInspectorList)
	{
		state._hdInspectorList = new TextList(8, 8, -4096, -4096);
		state._hdInspectorList->setSelectable(false);
		state._hdInspectorList->setScrolling(true, 0);
		state.add(state._hdInspectorList, "list", "craftInfo");
	}
	positionWidgets(state, state._hdWideLayout);
	const auto* generated = CalypsoF07CraftInfoGen::layoutForDesign(
		state._hdWideLayout ? 1280 : 740, state._hdWideLayout ? 720 : 360);
	if (!generated)
		CalypsoHdUiOverlay::instance().failHdRoute("F07 overview generated layout is missing");
	state.enableUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Tabbed list seam AFTER scaling: shares the inset track and native
	// scroll count/reveal with the painted slots.
	calypsoTabbedSeamList(state._hdInspectorList, state._window, *generated);
	auto* adapter = new CalypsoF07CraftInfoUi(&state);
	state._hdAdapter = adapter;
	CalypsoHdUiOverlay::instance().registerAdapter(adapter);
}

bool CalypsoF07CraftInfoUi::resize(CraftInfoState& state)
{
	if (!state._hdLayout) return false;
	const bool wide = f07HdWideLayout();
	state._hdWideLayout = wide;
	positionWidgets(state, wide);
	const auto* generated = CalypsoF07CraftInfoGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!generated) return false;
	state.recaptureUiScaling(generated->designWidth, generated->designHeight, 1.0f,
		/*subtractVanillaCenter=*/false);
	// Re-seam after recapture; same values preserve drag capture, real layout
	// changes reset it. Mount tiles recompose from live texts every
	// collect; selection never applies to the grid.
	calypsoTabbedSeamList(state._hdInspectorList, state._window, *generated);
	return true;
}

} // namespace Calypso
} // namespace OpenXcom
#endif
