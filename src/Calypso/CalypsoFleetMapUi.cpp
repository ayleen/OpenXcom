#ifdef __EMSCRIPTEN__
#include "CalypsoFleetMapUi.h"
#include "CalypsoFleetAlertLayout.h"
#include "CalypsoF21UiShared.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "Generated/CalypsoF18CourseMap.generated.h"
#include "Generated/CalypsoF18CraftNavigation.generated.h"
#include "CommandCenter/CommandCenterRenderer.h"
#include "../Geoscape/GeoscapeCraftState.h"
#include "../Engine/Game.h"
#include "../Geoscape/SelectDestinationState.h"
#include "../Interface/TextButton.h"

namespace OpenXcom { namespace Calypso {
namespace Map = CalypsoF18CourseMapGen;
namespace Nav = CalypsoF18CraftNavigationGen;

CalypsoFleetMapUi::~CalypsoFleetMapUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void *CalypsoFleetMapUi::topState() const { return _state; }

void CalypsoFleetMapUi::layout()
{
	_wide = calypsoFleetAlertWide();
	const auto &g = Map::kLayouts[_wide ? 0 : 1];
	auto &s = *_state;
	calypsoPlaceFleetWidget(*s._window, g.banner);
	calypsoPlaceFleetWidget(*s._txtTitle, g.title);
	calypsoPlaceFleetWidget(*s._lead, g.lead);
	calypsoPlaceFleetWidget(*s._fuel, g.fuel);
	calypsoPlaceFleetWidget(*s._range, g.reach);
	calypsoPlaceFleetWidget(*s._btnRotateLeft, g.left);
	calypsoPlaceFleetWidget(*s._btnRotateRight, g.right);
	calypsoPlaceFleetWidget(*s._btnRotateUp, g.up);
	calypsoPlaceFleetWidget(*s._btnRotateDown, g.down);
	calypsoPlaceFleetWidget(*s._btnZoomIn, g.zoomIn);
	calypsoPlaceFleetWidget(*s._btnZoomOut, g.zoomOut);
	calypsoPlaceFleetWidget(*s._btnCydonia, g.final);
	calypsoPlaceFleetWidget(*s._btnCancel, g.cancel);
	s.recaptureUiScaling(g.designWidth, g.designHeight, 1.0f, false);
	calypsoHdOperationsPublishHarnessVisibility();
}

namespace {
class FleetRails
{
public:
	FleetRails(CalypsoHdFrameBuilder &builder, const void *state, Mod *mod, Surface *banner,
		int designBannerWidth, bool wide)
		: _wide(wide), _painter{builder, 18, reinterpret_cast<std::uintptr_t>(state), 0, 1.0f, 1.0, {}}
	{
		auto &overlay = CalypsoHdUiOverlay::instance();
		if (!calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_SB", _heading)
			|| !calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_R", _body))
			overlay.failHdRoute("Fleet navigation fonts unavailable");
		const auto &metrics = overlay.frozenMetrics();
		_painter.sx = metrics.scaleX;
		_painter.sy = metrics.scaleY;
		_painter.uiScale = static_cast<double>(banner->getWidth()) / designBannerWidth;
		builder.beginSubgroup();
		panel(banner);
	}

	void panel(Surface *widget)
	{
		_painter.styled(f21WidgetRect(widget), f21WindowStyle(), widget, _role++);
	}

	void field(Text *widget, bool title = false, const std::string &overrideText = {})
	{
		if (!widget->getVisible()) return;
		const auto text = CommandCenter::calypsoHdNormalizeTtfDisplayText(overrideText.empty() ? widget->getText() : overrideText);
		_painter.text(widget, title ? _heading : _body, text, CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, _role++, 0.0,
			title ? (_wide ? 26 : 18) : (_wide ? 17 : 13), false);
	}

	void button(InteractiveSurface *owner, const std::string &label)
	{
		if (!owner->getVisible()) return;
		_painter.styled(f21WidgetRect(owner), f21ButtonStyleFor(CalypsoActionTone::Safe,
			f21ButtonVisualState(dynamic_cast<TextButton*>(owner))), owner, _role++);
		_painter.text(owner, _heading, label, CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, _role++, 0.0, _wide ? 17 : 14, false);
	}

private:
	bool _wide;
	std::uint32_t _role = 1;
	CalypsoTtfSourceDescriptor _heading, _body;
	CalypsoF21Painter _painter;
};
}

void CalypsoFleetMapUi::collect(CalypsoHdFrameBuilder &builder) const
{
	auto &s = *_state;
	const auto &g = Map::kLayouts[_wide ? 0 : 1];
	FleetRails rails(builder, &s, s._game->getMod(), s._window, g.banner.w, _wide);
	rails.field(s._txtTitle, true);
	rails.field(s._lead);
	rails.field(s._fuel);
	rails.field(s._range);
	rails.button(s._btnRotateLeft, "←");
	rails.button(s._btnRotateUp, "↑");
	rails.button(s._btnRotateDown, "↓");
	rails.button(s._btnRotateRight, "→");
	rails.button(s._btnZoomOut, "−");
	rails.button(s._btnZoomIn, "+");
	rails.button(s._btnCydonia, s._btnCydonia->getText());
	rails.button(s._btnCancel, s._btnCancel->getText());
}

CalypsoFleetNavigationUi::~CalypsoFleetNavigationUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}

const void *CalypsoFleetNavigationUi::topState() const { return _state; }

void CalypsoFleetNavigationUi::layout()
{
	auto &s = *_state;
	_wide = calypsoFleetAlertWide();
	const auto &g = Nav::kLayouts[_wide ? 0 : 1];
	calypsoPlaceFleetWidget(*s._window, g.banner);
	calypsoPlaceFleetWidget(*s._details, g.details);
	calypsoPlaceFleetWidget(*s._txtTitle, g.title);
	calypsoPlaceFleetWidget(*s._txtStatus, g.status);
	calypsoPlaceFleetWidget(*s._txtBase, g.base);
	calypsoPlaceFleetWidget(*s._txtSpeed, g.speed);
	calypsoPlaceFleetWidget(*s._txtMaxSpeed, g.maximum);
	calypsoPlaceFleetWidget(*s._txtAltitude, g.altitude);
	calypsoPlaceFleetWidget(*s._txtFuel, g.fuel);
	calypsoPlaceFleetWidget(*s._txtDamage, g.damage);
	calypsoPlaceFleetWidget(*s._txtShield, g.shield);
	calypsoPlaceFleetWidget(*s._txtSoldier, g.crew);
	calypsoPlaceFleetWidget(*s._txtHWP, g.vehicles);
	calypsoPlaceFleetWidget(*s._txtETA, g.eta);
	calypsoPlaceFleetWidget(*s._targetRange, g.distance);
	const Nav::CalypsoF18CraftNavigationGenRect slots[] = {g.slot1, g.slot2, g.slot3, g.slot4};
	static_assert(RuleCraft::WeaponMax == 4, "Update the navigation rail when the equipment slot limit changes.");
	for (int i = 0; i < s._weaponNum; ++i)
		calypsoPlaceFleetWidget(*s._txtWeaponName[i], slots[i]);
	calypsoPlaceFleetWidget(*s._btnBase, g.home);
	calypsoPlaceFleetWidget(*s._btnTarget, g.target);
	calypsoPlaceFleetWidget(*s._btnPatrol, g.patrol);
	calypsoPlaceFleetWidget(*s._btnCancel, g.cancel);
	s.recaptureUiScaling(g.designWidth, g.designHeight, 1.0f, false);
	calypsoHdOperationsPublishHarnessVisibility();
}

void CalypsoFleetNavigationUi::collect(CalypsoHdFrameBuilder &builder) const
{
	auto &s = *_state;
	const auto &g = Nav::kLayouts[_wide ? 0 : 1];
	FleetRails rails(builder, &s, s._game->getMod(), s._window, g.banner.w, _wide);
	rails.panel(s._details);
	rails.field(s._txtTitle, true);
	for (auto *field : {s._txtStatus, s._txtBase, s._txtSpeed, s._txtMaxSpeed, s._txtAltitude,
		s._txtFuel, s._txtDamage, s._txtShield, s._txtSoldier, s._txtHWP, s._txtETA, s._targetRange})
		rails.field(field);
	for (int i = 0; i < s._weaponNum; ++i)
	{
		std::string label = s._txtWeaponName[i]->getText();
		if (s._txtWeaponAmmo[i]->getVisible()) label += "  " + s._txtWeaponAmmo[i]->getText();
		rails.field(s._txtWeaponName[i], false, label);
	}
	for (auto *button : {s._btnBase, s._btnTarget, s._btnPatrol, s._btnCancel})
		rails.button(button, button->getText());
}

} }
#endif
