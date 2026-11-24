#ifdef __EMSCRIPTEN__
#include "CalypsoTacticalEquipment.h"
#include "../Battlescape/AlienInventory.h"
#include "../Battlescape/AlienInventoryState.h"
#include "../Battlescape/Inventory.h"
#include "../Battlescape/InventoryState.h"
#include "../Battlescape/WarningMessage.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Interface/BattlescapeButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextEdit.h"
#include "../Mod/Armor.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInventory.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/BattleUnit.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdImageSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "CalypsoHdUiOverlay.h"
#include "Generated/CalypsoF26Inspection.generated.h"
#include "Generated/CalypsoF26Inventory.generated.h"

namespace OpenXcom
{
namespace Calypso
{
namespace Gear = CalypsoF26InventoryGen;
namespace Inspect = CalypsoF26InspectionGen;
namespace
{
void place(Surface& surface, CalypsoLogicalRect r)
{
	surface.setX(r.x);
	surface.setY(r.y);
	surface.setWidth(r.w);
	surface.setHeight(r.h);
}
const char* label(const std::string& id)
{
	if (id == "gear")
		return "STR_EQUIPMENT";
	if (id == "info")
		return "STR_CAL_EQUIP_DETAILS";
	if (id == "previous")
		return "STR_CAL_EQUIP_PREVIOUS";
	if (id == "next")
		return "STR_CAL_EQUIP_NEXT";
	if (id == "ground")
		return "STR_CAL_EQUIP_GROUND";
	if (id == "unload")
		return "STR_CAL_EQUIP_UNLOAD";
	if (id == "stats")
		return "STR_STATISTICS";
	if (id == "links")
		return "STR_CAL_EQUIP_ACTIONS";
	if (id == "left")
		return "STR_LEFT_HAND";
	if (id == "right")
		return "STR_RIGHT_HAND";
	if (id == "armor")
		return "STR_ARMOR";
	return "STR_CAL_EQUIP_CLOSE";
}
}
CalypsoTacticalEquipment::CalypsoTacticalEquipment(InventoryState& s) : _owner(s), _inventory(&s)
{
	for (auto* surface : s._surfaces)
		surface->setX(-10000);
	open();
}
CalypsoTacticalEquipment::CalypsoTacticalEquipment(AlienInventoryState& s) : _owner(s), _inspection(&s)
{
	for (auto* surface : s._surfaces)
		surface->setX(-10000);
	s._inv->setVisible(false);
	s._btnArmor->setVisible(false);
	open();
}
void CalypsoTacticalEquipment::open()
{
	_session = calypsoHarnessSession().generation;
	const auto bind = [&](const char* id)
	{
		auto* button = new TextButton(1, 1, 0, 0);
		_owner.add(button);
		button->setText(_owner.tr(label(id)));
		const auto handler = _inventory ? (ActionHandler)&InventoryState::hdBoardClick : (ActionHandler)&AlienInventoryState::hdBoardClick;
		button->onMouseClick(handler);
		if (_inspection && std::string(id) == "done")
			button->onKeyboardPress(handler, Options::keyCancel);
		_actions.emplace(id, button);
	};
	bind("gear");
	bind("info");
	bind("done");
	if (_inventory && !_inventory->_tu)
		_actions.at("done")->setText(_owner.tr("STR_CAL_EQUIP_COMMIT"));
	if (_inventory)
		for (auto id : {"ground", "unload", "stats", "links", "previous", "next"})
			bind(id);
	else
		for (auto id : {"left", "right", "armor"})
			bind(id);
	_model.listWidget = new TextList(1, 1, 0, 0);
	_owner.add(_model.listWidget);
	_model.listWidget->setVisible(false);
	_model.titleWidget = new Text(1, 1, 0, 0);
	_owner.add(_model.titleWidget);
	_model.instance = &_owner;
	_model.mod = getCurrentGame()->getMod();
	_model.familyId = Gear::kFamilyId;
	_model.panelFillTop = Gear::kPanelFillTop;
	_model.panelFillBottom = Gear::kPanelFillBottom;
	_model.frameColor = Gear::kFrame;
	_model.selectedTabColor = Gear::kSelectedTab;
	_model.dividerColor = Gear::kDivider;
	_model.textColor = Gear::kText;
	_model.mutedTextColor = Gear::kMutedText;
	_model.scrollTrackColor = Gear::kPanelFillBottom;
	_model.cutCornerPx = Gear::kCutCornerPx;
	layout();
	refresh();
	CalypsoHdUiOverlay::instance().registerAdapter(this);
	if (calypsoHarnessHostUp(calypsoHarnessSession()))
		calypsoHdHarnessDomShow();
}
CalypsoTacticalEquipment::~CalypsoTacticalEquipment()
{
	calypsoHdHarnessTeardownForTarget(&_owner, _session);
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}
void CalypsoTacticalEquipment::layout()
{
	auto& m = _model;
	const auto vp = calypsoHdOperationsEffectiveViewport();
	m.wide = vp.cssWidth >= 1000 && vp.cssHeight >= 550;
	const int index = m.wide ? 0 : 1;
	auto layout = [&](const auto& g, const auto& tools, int toolCount, const auto& actions, int actionCount, const auto& footers)
	{
		m.designWidth = g.designWidth;
		m.designHeight = g.designHeight;
		const double fit = std::min(double(vp.cssWidth) / g.designWidth, double(vp.cssHeight) / g.designHeight);
		const double sx = fit * vp.projectionLogicalWidth / vp.cssWidth, sy = fit * vp.projectionLogicalHeight / vp.cssHeight;
		const int ox = (vp.projectionLogicalWidth - std::lround(g.designWidth * sx)) / 2;
		const int oy = (vp.projectionLogicalHeight - std::lround(g.designHeight * sy)) / 2;
		auto rect = [&](int x, int y, int w, int h)
		{ return CalypsoLogicalRect{ox + int(std::lround(x * sx)), oy + int(std::lround(y * sy)), std::max(1, int(std::lround(w * sx))), std::max(1, int(std::lround(h * sy)))}; };
		auto project = [&](const auto& r)
		{ return rect(r.x, r.y, r.w, r.h); };
		m.window = project(g.window);
		m.title = project(g.title);
		m.footer = project(g.footer);
		m.titleDesignHeight = g.title.h;
		m.uiScale = sx;
		m.summaryBar = project(g.summaryBar);
		m.summary = {{"", m.summaryBar}};
		const int top = g.tabBar.y, height = g.footer.y - top - 6;
		const int rightWidth = m.wide ? 292 : 226;
		const int rightX = g.title.x + g.title.w - rightWidth;
		const int boardWidth = std::min(g.title.w - rightWidth - 10, height * 320 / 200);
		const int boardHeight = boardWidth * 200 / 320;
		const int boardX = g.title.x + (rightX - g.title.x - boardWidth) / 2;
		const int boardY = top + (height - boardHeight) / 2;
		m.collectionViewport = rect(g.title.x, top, rightX - g.title.x - 4, height);
		m.plotRect = rect(boardX, boardY, boardWidth, boardHeight);
		m.tabBar = {};
		m.toolbarBar = {};
		m.tabs.clear();
		m.toolbar.clear();
		m.actions.clear();
		const int gap = 4, bw = (rightWidth - gap) / 2, bh = m.wide ? 52 : 44;
		auto controlRect = [&](int i)
		{ return rect(rightX + (i % 2) * (bw + gap), top + (i / 2) * (bh + gap), bw, bh); };
		for (int i = 0; i < 2; ++i)
		{
			const char* id = i ? "info" : "gear";
			auto* button = _actions.at(id);
			button->setVisible(!_inspection || !Options::oxceDisableAlienInventory);
			place(*button, controlRect(i));
			if (button->getVisible())
				m.tabs.push_back({id, button->getText(), bool(i) == _details, true, button, controlRect(i)});
		}
		auto add = [&](auto& out, const auto& spec, CalypsoLogicalRect r)
		{
			auto* button = _actions.at(spec.id);
			bool enabled = true;
			if (_inspection && std::string(spec.id) != "done")
			{
				auto* unit = _inspection->_inv->getSelectedUnit();
				enabled = !Options::oxceDisableAlienInventory && unit;
				if (enabled && std::string(spec.id) == "left")
					enabled = unit->getLeftHandWeapon() != nullptr;
				if (enabled && std::string(spec.id) == "right")
					enabled = unit->getRightHandWeapon() != nullptr;
			}
			if (_inventory)
			{
				auto& inv = *_inventory->_inv;
				const std::string id = spec.id;
				const auto* item = inv.getSelectedItem();
				const char* text = label(id);
				if (id == "done")
					text = item ? "STR_CAL_EQUIP_CANCEL_ITEM" : _inventory->_tu ? label(id)
																				: "STR_CAL_EQUIP_COMMIT";
				else if (id == "ground" && item)
					text = "STR_CAL_EQUIP_DROP";
				else if (id == "stats" && item)
					text = "STR_UFOPAEDIA";
				else if (id == "links" && item)
				{
					const bool primed = item->getFuseTimer() >= 0;
					text = primed ? "STR_CAL_EQUIP_UNPRIME" : "STR_CAL_EQUIP_PRIME";
					enabled = !inv._tu && (!inv._base || Options::includePrimeStateInSavedLayout) && item->getRules()->getFuseTimerType() != BFT_NONE && (primed ? item->getRules()->getCostUnprime().Time : item->getRules()->getCostPrime().Time) > 0;
				}
				else if (id == "links")
					enabled = _inventory->_btnLinks->getVisible();
				else if ((id == "previous" || id == "next") && item)
					enabled = false;
				button->setText(_owner.tr(text));
			}
			button->setVisible(enabled);
			place(*button, r);
			if (enabled)
				out.push_back({button, nullptr, button->getText(), r, calypsoTabbedToneForContract(spec.tone), spec.fill, spec.border, spec.text});
		};
		for (int i = 0; i < toolCount; ++i)
			add(m.toolbar, tools[i], controlRect(i + 2));
		for (int i = 0; i < actionCount; ++i)
			add(m.actions, actions[i], project(footers[i].rect));
		if (_inventory)
		{
			place(*_inventory->_inv, m.plotRect);
			_inventory->_inv->setVisible(!_details);
			place(*_inventory->_txtName, rect(g.title.x, g.title.y, g.title.w / 2, g.title.h));
			place(*_inventory->_btnQuickSearch, rect(rightX, top + 3 * (bh + gap), rightWidth, bh));
		}
	};
	if (_inventory)
		layout(Gear::kLayouts[index], Gear::kToolbar, Gear::kToolbarCount, Gear::kActions, Gear::kActionCount, Gear::kActionRects[index]);
	else
		layout(Inspect::kLayouts[index], Inspect::kToolbar, Inspect::kToolbarCount, Inspect::kActions, Inspect::kActionCount, Inspect::kActionRects[index]);
}
void CalypsoTacticalEquipment::refresh()
{
	if (_inspection && Options::oxceDisableAlienInventory)
		return;
	const auto revision = _inventory ? _inventory->_inv->_hdItemRevision : _inspection->_inv->_animFrame;
	const auto* held = _inventory ? _inventory->_inv->_selItem : nullptr;
	const int x = held ? _inventory->_inv->_selection->getX() : 0, y = held ? _inventory->_inv->_selection->getY() : 0;
	if (_model.plotImage.pixels && _revision == revision && _held == held && _heldX == x && _heldY == y)
		return;
	_revision = revision;
	if (_inventory && _held != held)
		layout();
	_held = held;
	_heldX = x;
	_heldY = y;
	Surface canvas(320, 200);
	Surface* art = _inventory ? _inventory->_soldier : _inspection->_soldier;
	if (auto* palette = art->getEffectivePalette())
		canvas.setPalette(palette);
	art->blitNShade(&canvas, _inventory ? 0 : _model.mod->getAlienInventoryOffsetX(), 0);
	if (_inventory)
	{
		auto& inv = *_inventory->_inv;
		inv._items->blitNShade(&canvas, 0, 0);
		if (inv._selItem)
			inv._selection->blitNShade(&canvas, inv._selection->getX(), inv._selection->getY());
	}
	else
		_inspection->_inv->_items->blitNShade(&canvas, 0, 0);
	std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> rgba(SDL_ConvertSurfaceFormat(canvas.getSurface(), SDL_PIXELFORMAT_RGBA32, 0), SDL_FreeSurface);
	if (!rgba || !rgba->pixels || rgba->pitch != 320 * 4)
		CalypsoHdUiOverlay::instance().failHdRoute("Equipment image unavailable");
	const auto* bytes = static_cast<const Uint8*>(rgba->pixels);
	const size_t size = 320 * 200 * 4;
	auto& image = _model.plotImage;
	if (image.pixels && std::equal(bytes, bytes + size, image.pixels->px.begin()))
		return;
	auto pixels = std::make_shared<CalypsoHdImageRgba>();
	pixels->w = 320;
	pixels->h = 200;
	pixels->px.assign(bytes, bytes + size);
	static std::uint64_t generation = 0;
	image.source = _inventory ? "calypso/inventory" : "calypso/inspection";
	image.generation = ++generation;
	image.pixels = std::move(pixels);
}
void CalypsoTacticalEquipment::click(Action* action)
{
	auto it = std::find_if(_actions.begin(), _actions.end(), [&](const auto& entry)
						   { return entry.second == action->getSender(); });
	if (it == _actions.end())
		return;
	const auto& id = it->first;
	if (_inventory)
	{
		auto& inv = *_inventory->_inv;
		auto* item = inv.getSelectedItem();
		if (item && (id == "done" || id == "ground" || id == "stats" || id == "links"))
		{
			if (id == "done")
				inv.cancelHeldItem();
			else if (id == "ground")
				inv.quickDrop();
			else if (id == "stats")
			{
				auto article = item->getRules()->getUfopediaType();
				Ufopaedia::openArticle(getCurrentGame(), article);
				return;
			}
			else
			{
				inv.cancelHeldItem();
				inv.toggleInventoryFuse(item);
			}
			_inventory->updateStats();
			refresh();
			return;
		}
	}
	if (id == "done")
	{
		if (_inventory)
			_inventory->btnOkClick(action);
		else
			_inspection->btnOkClick(action);
		return;
	}
	if (id == "gear" || id == "info")
	{
		_details = id == "info";
		layout();
		return;
	}
	if (_inventory)
	{
		if (id == "previous")
			_inventory->btnPrevClick(action);
		else if (id == "next")
			_inventory->btnNextClick(action);
		else if (id == "ground")
			_inventory->btnGroundClickForward(action);
		else if (id == "unload")
			_inventory->btnUnloadClick(action);
		else if (id == "stats")
			_inventory->btnRankClick(action);
		else if (id == "links")
			_inventory->btnLinksClick(action);
		return;
	}
	auto* unit = _inspection->_inv->getSelectedUnit();
	if (!unit || Options::oxceDisableAlienInventory)
		return;
	if (id == "armor")
		_inspection->btnArmorClickMiddle(action);
	else
	{
		auto* item = id == "left" ? unit->getLeftHandWeapon() : unit->getRightHandWeapon();
		if (item)
		{
			auto article = item->getRules()->getUfopediaType();
			Ufopaedia::openArticle(getCurrentGame(), article);
		}
	}
}
void CalypsoTacticalEquipment::updateData(CalypsoTabbedModel& m) const
{
	const auto board = m.plotRect;
	auto labelAt = [&](int x, int y, int w, int h, const std::string& value)
	{
		if (value.empty())
			return;
		m.plotLabels.push_back({value, {board.x + x * board.w / 320, board.y + y * board.h / 200, std::max(1, w * board.w / 320), std::max(1, h * board.h / 200)}});
	};
	if (_inspection)
	{
		m.titleText = _inspection->_txtName->getText();
		if (Options::oxceDisableAlienInventory)
		{
			m.titleText = _owner.tr("STR_INVENTORY");
			m.summary[0].text = _inspection->_txtName->getText();
			return;
		}
		auto* unit = _inspection->_inv->getSelectedUnit();
		if (!unit)
			return;
		if (const auto* armor = unit->getArmor())
		{
			const auto& type = armor->getType();
			const std::string translated = _owner.tr(type);
			m.summary[0].text = translated == type ? std::string(_owner.tr("STR_EQUIPMENT")) : translated;
		}
		if (!_details)
		{
			for (const auto& pair : *m.mod->getInventories())
			{
				const auto* slot = pair.second;
				if (slot->getType() != INV_HAND)
					continue;
				const int offset = _inspection->_inv->_dynamicOffset;
				const int x = slot->getX() + m.mod->getAlienInventoryOffsetX() + (slot->isRightHand() ? -offset : slot->isLeftHand() ? offset
																																	 : 0);
				const int y = slot->getY();
				const int w = RuleInventory::HAND_W * RuleInventory::SLOT_W;
				const int h = RuleInventory::HAND_H * RuleInventory::SLOT_H;
				m.plotCells.push_back({board.x + x * board.w / 320, board.y + y * board.h / 200,
									   std::max(1, w * board.w / 320), std::max(1, h * board.h / 200)});
				labelAt(x, std::max(0, y - 12), 85, 12, _owner.tr(slot->getId()));
			}
		}
		if (_details)
		{
			int y = 0;
			auto row = [&](const std::string& s)
			{labelAt(4,y,312,17,s);y+=18; };
			for (auto* hand : {unit->getLeftHandWeapon(), unit->getRightHandWeapon()})
				if (hand)
					row(_owner.tr(hand->getRules()->getType()));
			if (unit->indicatorsAreEnabled())
			{
				for (int i = 0; i < BODYPART_MAX; ++i)
					if (unit->getFatalWound((UnitBodyPart)i))
						row(std::string(_owner.tr(PARTS_STRING[i])) + " " + std::to_string(unit->getFatalWound((UnitBodyPart)i)));
				if (unit->getFire())
					row(_owner.tr("STR_CAL_EQUIP_BURNING"));
			}
			if (Options::debug)
			{
				row(_inspection->_txtLeftHand->getText());
				row(_inspection->_txtRightHand->getText());
			}
		}
		return;
	}
	auto& s = *_inventory;
	m.titleText = s._txtName->getVisible() ? s._txtName->getText() : s._txtNameStatic->getText();
	m.summary[0].text = s._inv->_warning->getVisible() ? s._inv->_warning->hdText() : s._txtItem->getText();
	for (auto* field : {s._txtTus, s._txtWeight})
		if (field->getVisible())
			m.summary[0].text += "   " + field->getText();
	if (_details)
	{
		int y = 0;
		for (auto* field : {s._txtPosition, s._txtTus, s._txtWeight, s._txtStatLine1, s._txtStatLine2, s._txtStatLine3, s._txtStatLine4, s._txtItem, s._txtAmmo})
			if (field->getVisible() && !field->getText().empty())
			{
				labelAt(4, y, 312, 18, field->getText());
				y += 19;
			}
	}
	else
		for (const auto& name : m.mod->getInvsList())
		{
			const auto* slot = m.mod->getInventory(name, true);
			auto cell = [&](int x, int y, int w, int h)
			{
				m.plotCells.push_back({board.x + x * board.w / 320, board.y + y * board.h / 200,
									   std::max(1, (x + w) * board.w / 320 - x * board.w / 320), std::max(1, (y + h) * board.h / 200 - y * board.h / 200)});
			};
			const int cw = RuleInventory::SLOT_W, ch = RuleInventory::SLOT_H;
			if (slot->getType() == INV_SLOT)
				for (const auto& spot : *slot->getSlots())
					cell(slot->getX() + spot.x * cw, slot->getY() + spot.y * ch, cw, ch);
			else if (slot->getType() == INV_HAND)
				cell(slot->getX(), slot->getY(), cw * RuleInventory::HAND_W, ch * RuleInventory::HAND_H);
			else if (slot->getType() == INV_GROUND)
				for (int x = slot->getX(); x + cw <= 320; x += cw)
					for (int y = slot->getY(); y + ch <= 200; y += ch)
						cell(x, y, cw, ch);
			const auto& inv = *s._inv;
			std::string text = _owner.tr(name).arg(1 + inv._groundOffset / inv._groundSlotsX).arg(1 + inv._xMax / inv._groundSlotsX);
			if (inv._selItem && inv._selItem->getSlot() != slot)
				text += " " + std::to_string(inv._selItem->getMoveToCost(slot));
			labelAt(slot->getX(), std::max(0, slot->getY() - 10), 90, 10, text);
		}
	if (s._btnQuickSearch->getVisible())
	{
		auto* field = s._btnQuickSearch;
		m.controls.push_back({"search", "text", field->getText().empty() ? std::string(_owner.tr("STR_QUICK_SEARCH")) : field->getText(), field, {field->getX(), field->getY(), field->getWidth(), field->getHeight()}});
	}
}
void CalypsoTacticalEquipment::collect(CalypsoHdFrameBuilder& builder) const
{
	auto model = _model;
	const auto& metrics = CalypsoHdUiOverlay::instance().frozenMetrics();
	model.projectionScaleX = model.uiScale * metrics.scaleX;
	model.projectionScaleY = model.uiScale * metrics.scaleY;
	model.textRasterScale = std::max(1.0, model.projectionScaleY);
	updateData(model);
	if (_details)
		model.plotImage = {};
	calypsoCollectTabbedManagement(builder, model, _motion);
}
} // namespace Calypso
std::string WarningMessage::hdText() const { return _text->getText(); }
void InventoryState::hdBoardClick(Action* action) { _hdBoard->click(action); }
void AlienInventoryState::think()
{
	State::think();
	if (_hdBoard)
		_hdBoard->refresh();
}
void AlienInventoryState::hdBoardClick(Action* action) { _hdBoard->click(action); }
} // namespace OpenXcom
#endif
