#ifdef __EMSCRIPTEN__
#include "CalypsoFleetMapUi.h"
#include "CalypsoFleetAlertLayout.h"
#include "CalypsoF21UiShared.h"
#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsChrome.h"
#include "Generated/CalypsoF18CourseMap.generated.h"
#include "../Engine/Game.h"
#include "../Geoscape/SelectDestinationState.h"
#include "../Interface/TextButton.h"

namespace OpenXcom { namespace Calypso {
namespace Map = CalypsoF18CourseMapGen;

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

void CalypsoFleetMapUi::collect(CalypsoHdFrameBuilder &builder) const
{
	auto &s = *_state;
	auto &overlay = CalypsoHdUiOverlay::instance();
	CalypsoTtfSourceDescriptor heading, body;
	if (!calypsoHdResolveFontDescriptor(s._game->getMod(), "FONT_CC_INTER_SB", heading)
		|| !calypsoHdResolveFontDescriptor(s._game->getMod(), "FONT_CC_INTER_R", body))
		overlay.failHdRoute("Course map fonts unavailable");
	const auto &g = Map::kLayouts[_wide ? 0 : 1];
	const auto &metrics = overlay.frozenMetrics();
	const auto banner = f21WidgetRect(s._window);
	const double scale = static_cast<double>(banner.w) / g.banner.w;
	builder.beginSubgroup();
	CalypsoF21Painter painter{builder, 18, reinterpret_cast<std::uintptr_t>(&s), 0, 1.0f, 1.0,
		{0, 0, g.designWidth, g.designHeight}, metrics.scaleX, metrics.scaleY};
	painter.uiScale = scale;
	painter.styled(banner, f21WindowStyle(), s._window, 1);
	std::uint32_t role = 2;
	auto field = [&](Text *text, bool title) {
		painter.text(text, title ? heading : body, text->getText(), CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Left, CalypsoHdVAlign::Middle, 1, role++, 0.0, title ? (_wide ? 26 : 18) : (_wide ? 18 : 14));
	};
	field(s._txtTitle, true);
	field(s._lead, false);
	field(s._fuel, false);
	field(s._range, false);
	auto button = [&](InteractiveSurface *owner, const std::string &label) {
		if (!owner->getVisible()) return;
		painter.styled(f21WidgetRect(owner), f21ButtonStyleFor(CalypsoActionTone::Safe,
			f21ButtonVisualState(dynamic_cast<TextButton*>(owner))), owner, role++);
		painter.text(owner, heading, label, CalypsoHdTheme::kNearWhite,
			CalypsoHdHAlign::Center, CalypsoHdVAlign::Middle, 1, role++, 0.0, _wide ? 18 : 15);
	};
	button(s._btnRotateLeft, "←");
	button(s._btnRotateUp, "↑");
	button(s._btnRotateDown, "↓");
	button(s._btnRotateRight, "→");
	button(s._btnZoomOut, "−");
	button(s._btnZoomIn, "+");
	button(s._btnCydonia, s._btnCydonia->getText());
	button(s._btnCancel, s._btnCancel->getText());
}

} }
#endif
