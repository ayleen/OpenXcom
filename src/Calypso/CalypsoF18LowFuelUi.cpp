#ifdef __EMSCRIPTEN__
#include "CalypsoF18LowFuelUi.h"
#include "CalypsoFleetAlertLayout.h"
#include "Generated/CalypsoF18LowFuel.generated.h"
#include "../Engine/Game.h"
#include "../Geoscape/LowFuelState.h"
#include "../Geoscape/GeoscapeState.h"

namespace OpenXcom { namespace Calypso {
namespace G = CalypsoF18LowFuelGen;
CalypsoF18LowFuelUi::~CalypsoF18LowFuelUi()
{
	CalypsoHdUiOverlay::instance().clearAdapter(this);
}
const void *CalypsoF18LowFuelUi::topState() const { return _state; }

void CalypsoF18LowFuelUi::collect(CalypsoHdFrameBuilder &builder) const
{
	auto &s = *_state;
	const int index = s._hdWideLayout ? 0 : 1;
	CalypsoSmallConfirmationModel m;
	m.instance = _state;
	m.mod = s._game->getMod();
	m.wide = s._hdWideLayout;
	m.familyId = G::kFamilyId;
	m.protocolText = G::kProtocol;
	m.titleText = s.tr("STR_CAL_FLEET_WARNING");
	m.visualScale = G::kPresentationScale;
	m.cutCornerPx = G::kCutCornerPx;
	m.protocolTextInsetPx = G::kProtocolTextInsetPx;
	m.panelFillTop = G::kPanelFillTop;
	m.panelFillBottom = G::kPanelFillBottom;
	m.frameColor = G::kFrame;
	m.protocolColor = G::kProtocolText;
	m.dividerColor = G::kDivider;
	m.footerDotColor = G::kFooterDot;
	m.warningColor = G::kWarning;
	m.motionDurationMs = G::kMotionDurationMs;
	m.motionScaleFrom = G::kMotionScaleFrom;
	calypsoCollectFleetAlert(builder, m, _motion, G::kLayouts[index], G::kButtonRects[index],
		G::kButtons, *s._window, *s._txtMessage, s._txtTitle, *s._btnOk, *s._btnOk5Secs);
}

void CalypsoF18LowFuelUi::configure(LowFuelState &s, bool allow)
{
	if (!allow || !s._game || !s._game->getMod())
		CalypsoHdUiOverlay::instance().failHdRoute("Fleet alert requires a loaded mod");
	auto *ui = new CalypsoF18LowFuelUi(&s);
	auto &overlay = CalypsoHdUiOverlay::instance();
	const auto *globe = dynamic_cast<GeoscapeState*>(s._game->getTopState());
	ui->_underlay = overlay.hasRegisteredState(globe) ? globe : nullptr;
	s._screen = !ui->_underlay;
	s._hdAdapter = ui;
	s._hdLayout = true;
	resize(s);
	overlay.registerAdapter(ui);
	if (calypsoHarnessHostUp(calypsoHarnessSession())) calypsoHdHarnessDomShow();
}

bool CalypsoF18LowFuelUi::resize(LowFuelState &s)
{
	if (!s._hdLayout) return false;
	s._hdWideLayout = calypsoFleetAlertWide();
	const int index = s._hdWideLayout ? 0 : 1;
	const auto &g = G::kLayouts[index];
	calypsoPlaceFleetAlert(g, G::kButtonRects[index], *s._window, *s._txtMessage,
		s._txtTitle, *s._btnOk, *s._btnOk5Secs);
	s.recaptureUiScaling(g.designWidth, g.designHeight, 1.0f, false);
	return true;
}
} }
#endif
