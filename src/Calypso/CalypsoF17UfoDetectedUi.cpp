#ifdef __EMSCRIPTEN__
#include "CalypsoF17UfoDetectedUi.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Geoscape/UfoDetectedState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Geoscape/Globe.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Ufo.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF17UfoDetected.generated.h"
#include "CalypsoHdHarnessHostState.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
#include "CalypsoViewportMailbox.h"
#include "CalypsoViewportModel.h"

namespace OpenXcom { namespace Calypso {

namespace
{

/// Map the engine direction key to the eight-way schematic course word.
std::string boardCourseWord(const std::string& directionKey)
{
	static const struct { const char* key; const char* word; } table[] = {
		{"STR_NORTH", "N"}, {"STR_NORTH_EAST", "NE"}, {"STR_EAST", "E"},
		{"STR_SOUTH_EAST", "SE"}, {"STR_SOUTH", "S"}, {"STR_SOUTH_WEST", "SW"},
		{"STR_WEST", "W"}, {"STR_NORTH_WEST", "NW"}, {"STR_NONE_UC", "NONE"},
	};
	for (const auto& entry : table)
	{
		if (directionKey == entry.key) return entry.word;
	}
	return "NONE";
}

/// Shortest wrapped longitude delta in [-PI, PI].
double wrapLongitudeDelta(double lon, double center)
{
	double delta = std::fmod(lon - center + 3.0 * M_PI, 2.0 * M_PI);
	if (delta < 0.0) delta += 2.0 * M_PI;
	return delta - M_PI;
}

CalypsoActionTone generatedActionTone(const char* tone)
{
	if (tone && std::strcmp(tone, "primary") == 0)
		return CalypsoActionTone::Primary;
	if (tone && std::strcmp(tone, "danger") == 0)
		return CalypsoActionTone::Destructive;
	return CalypsoActionTone::Safe;
}

} // namespace

CalypsoF17UfoDetectedUi::CalypsoF17UfoDetectedUi(UfoDetectedState* s, LayoutVariant layout)
	: _state(s), _layout(layout) {}

int CalypsoF17UfoDetectedUi::layoutIndex(LayoutVariant layout)
{
	switch (layout)
	{
	case LayoutVariant::Wide: return 0;
	case LayoutVariant::Portrait: return 2;
	case LayoutVariant::Compact: break;
	}
	return 1;
}

int CalypsoF17UfoDetectedUi::designWidth(LayoutVariant layout)
{
	switch (layout)
	{
	case LayoutVariant::Wide: return 1280;
	case LayoutVariant::Portrait: return 360;
	case LayoutVariant::Compact: break;
	}
	return 740;
}

int CalypsoF17UfoDetectedUi::designHeight(LayoutVariant layout)
{
	switch (layout)
	{
	case LayoutVariant::Wide: return 720;
	case LayoutVariant::Portrait: return 740;
	case LayoutVariant::Compact: break;
	}
	return 360;
}

CalypsoF17UfoDetectedUi::LayoutVariant CalypsoF17UfoDetectedUi::chooseLayout()
{
	// The harness request is authoritative while a preview is up: it must be
	// able to pin every variant (incl. Portrait) regardless of the live
	// browser viewport, exactly like the other family harnesses.
	const CalypsoHarnessSession& session = calypsoHarnessSession();
	if (session.hostUp && session.layoutExplicit)
	{
		switch (session.requestedLayout)
		{
		case CalypsoLayoutClass::Wide: return LayoutVariant::Wide;
		case CalypsoLayoutClass::Portrait: return LayoutVariant::Portrait;
		case CalypsoLayoutClass::Compact: break;
		}
		return LayoutVariant::Compact;
	}

	// Live viewport: an explicit or derived portrait orientation wins, then
	// the established wide threshold; everything else stays compact.
	const CalypsoViewportState& viewport = calypsoHdViewportModel().state();
	if (viewport.orientation == CalypsoOrientation::Portrait)
		return LayoutVariant::Portrait;
	if (viewport.orientation == CalypsoOrientation::Unknown
		&& Options::baseYResolution > Options::baseXResolution)
		return LayoutVariant::Portrait;
	if (calypsoViewportLayoutWidth(viewport, Options::baseXResolution) >= 1000)
		return LayoutVariant::Wide;
	return LayoutVariant::Compact;
}

void CalypsoF17UfoDetectedUi::applyLayout(UfoDetectedState& state, LayoutVariant layout)
{
	const auto* layoutGen = CalypsoF17UfoDetectedGen::layoutForDesign(
		designWidth(layout), designHeight(layout));
	if (!layoutGen)
		CalypsoHdUiOverlay::instance().failHdRoute(
			"F17 UFO contact layout is unavailable");

	// Safe-area translation: shift the whole generated composition (window +
	// the three input widgets by the SAME delta) inside the design-space safe
	// rect. Never rescale piecewise; collect() projects everything relative
	// to the actual window, so one translation moves the entire card.
	const CalypsoViewportState& viewport = calypsoHdViewportModel().state();
	const int designW = designWidth(layout);
	const int designH = designHeight(layout);
	const int safeLeft = viewport.logicalWidth > 0
		? (int)((double)viewport.safeLeft * designW / viewport.logicalWidth) : 0;
	const int safeRight = viewport.logicalWidth > 0
		? (int)((double)viewport.safeRight * designW / viewport.logicalWidth) : 0;
	const int safeTop = viewport.logicalHeight > 0
		? (int)((double)viewport.safeTop * designH / viewport.logicalHeight) : 0;
	const int safeBottom = viewport.logicalHeight > 0
		? (int)((double)viewport.safeBottom * designH / viewport.logicalHeight) : 0;

	int dx = 0;
	int dy = 0;
	if (layoutGen->window.x < safeLeft)
		dx = safeLeft - layoutGen->window.x;
	if (layoutGen->window.x + dx + layoutGen->window.w > designW - safeRight)
		dx = designW - safeRight - layoutGen->window.x - layoutGen->window.w;
	if (layoutGen->window.y < safeTop)
		dy = safeTop - layoutGen->window.y;
	if (layoutGen->window.y + dy + layoutGen->window.h > designH - safeBottom)
		dy = designH - safeBottom - layoutGen->window.y - layoutGen->window.h;

	auto applyRect = [](auto* widget, const auto& rect, int shiftX, int shiftY)
	{
		if (!widget) return;
		widget->setX(rect.x + shiftX);
		widget->setY(rect.y + shiftY);
		widget->setWidth(rect.w);
		widget->setHeight(rect.h);
	};
	applyRect(state._window, layoutGen->window, dx, dy);
	const int index = layoutIndex(layout);
	applyRect(state._btnIntercept, CalypsoF17UfoDetectedGen::kButtonRects[index][0].rect, dx, dy);
	applyRect(state._btnCentre, CalypsoF17UfoDetectedGen::kButtonRects[index][1].rect, dx, dy);
	applyRect(state._btnCancel, CalypsoF17UfoDetectedGen::kButtonRects[index][2].rect, dx, dy);
}

CalypsoF17UfoDetectedUi::~CalypsoF17UfoDetectedUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF17UfoDetectedUi::topState() const { return _state; }

void CalypsoF17UfoDetectedUi::collect(CalypsoHdFrameBuilder& builder) const
{
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    const int layoutIdx = layoutIndex(_layout);
    const auto* g = CalypsoF17UfoDetectedGen::layoutForDesign(
        designWidth(_layout), designHeight(_layout));
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,g->designWidth,g->designHeight};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](const auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    auto factPart = [&](int index, bool value) -> const CalypsoF17UfoDetectedGen::CalypsoF17UfoDetectedGenRect&
    {
        switch (index)
        {
            case 1: return value ? g->fact1Value : g->fact1Label;
            case 2: return value ? g->fact2Value : g->fact2Label;
            case 3: return value ? g->fact3Value : g->fact3Label;
            case 4: return value ? g->fact4Value : g->fact4Label;
            default: return value ? g->fact5Value : g->fact5Label;
        }
    };

    CalypsoContactIntelBoardModel m{};
    m.familyId = CalypsoF17UfoDetectedGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod();
    switch (_layout)
    {
        case LayoutVariant::Wide: m.layout = CalypsoContactIntelLayout::Wide; break;
        case LayoutVariant::Portrait: m.layout = CalypsoContactIntelLayout::Portrait; break;
        case LayoutVariant::Compact: m.layout = CalypsoContactIntelLayout::Compact; break;
    }
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    const CalypsoHdPresentationMetrics& presentation =
        CalypsoHdUiOverlay::instance().frozenMetrics();
    m.uiScale = uiScale;
    m.visualScale = CalypsoF17UfoDetectedGen::kPresentationScale;
    m.projectionScaleX = uiScale * presentation.scaleX;
    m.projectionScaleY = uiScale * presentation.scaleY;
    m.window = proj(g->window); m.status = proj(g->status);
    m.plotPanel = proj(g->plotPanel); m.plotArea = proj(g->plotArea);
    m.reportPanel = proj(g->reportPanel);
    m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message);
    m.footer = proj(g->footer); m.note = m.footer;
    for (int i = 1; i <= 5; ++i)
    {
        m.factRects.push_back(proj(factPart(i, false)));
        m.factRects.push_back(proj(factPart(i, true)));
    }
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtUfo;
    m.messageWidget = nullptr;
    m.protocolWidget = _state->_txtDetected;
    m.protocolText.clear();
    m.noteText.clear();
    m.warningGlyph.clear();
    m.protocolTextInsetPx = CalypsoF17UfoDetectedGen::kProtocolTextInsetPx;
    m.cutCornerPx = CalypsoF17UfoDetectedGen::kCutCornerPx;
    m.innerRadiusPx = CalypsoF17UfoDetectedGen::kInnerRadiusPx;
    m.panelFillTop = CalypsoF17UfoDetectedGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF17UfoDetectedGen::kPanelFillBottom;
    m.frameColor = CalypsoF17UfoDetectedGen::kFrame;
    m.protocolColor = CalypsoF17UfoDetectedGen::kProtocolText;
    m.dividerColor = CalypsoF17UfoDetectedGen::kDivider;
    m.footerDotColor = CalypsoF17UfoDetectedGen::kFooterDot;
    m.warningColor = CalypsoF17UfoDetectedGen::kWarning;
    m.backdropColor = _layout == LayoutVariant::Wide
        ? CalypsoF17UfoDetectedGen::kBackdropWide
        : (_layout == LayoutVariant::Portrait
            ? CalypsoF17UfoDetectedGen::kBackdropPortrait
            : CalypsoF17UfoDetectedGen::kBackdropCompact);
    m.radarRingColor = CalypsoF17UfoDetectedGen::kRadarRing;
    m.radarStrongRingColor = CalypsoF17UfoDetectedGen::kRadarRingStrong;
    m.radarAxisColor = CalypsoF17UfoDetectedGen::kRadarAxis;
    m.radarSweepColor = CalypsoF17UfoDetectedGen::kRadarSweep;
    m.radarSweepPeriodMs = CalypsoF17UfoDetectedGen::kRadarSweepPeriodMs;
    m.factLabelColor = CalypsoF17UfoDetectedGen::kFactLabel;
    m.factValueColor = CalypsoF17UfoDetectedGen::kFactValue;
    m.plotFrameColor = CalypsoF17UfoDetectedGen::kPlotFrame;
    m.plotContactColor = CalypsoF17UfoDetectedGen::kRadarContact;
    m.plotContactHaloColor = CalypsoF17UfoDetectedGen::kRadarContactHalo;
    m.plotBaseColor = CalypsoF17UfoDetectedGen::kRadarBase;
    m.plotCourseColor = CalypsoF17UfoDetectedGen::kRadarCourse;
    m.factDividerColor = CalypsoF17UfoDetectedGen::kFactDivider;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF17UfoDetectedGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF17UfoDetectedGen::kMotionScaleFrom;

    // Runtime content: the vanilla state and its contact remain authoritative.
    Ufo* ufo = _state->_ufo;
    m.titleText = _state->_txtUfo ? _state->_txtUfo->getText() : std::string();
    m.protocolText = CalypsoF17UfoDetectedGen::kProtocol;
    m.messageText.clear();

    // Radar: a SCHEMATIC circular bearing plot. The nearest base sits at the
    // center; the contact lands on the true base->contact direction at a
    // fixed fraction of the radar radius. Distance stays in the DIST fact
    // row -- it never becomes a pixel radius.
    const CalypsoLogicalRect& plot = m.plotArea;
    const int centerX = plot.x + plot.w / 2;
    const int centerY = plot.y + plot.h / 2;
    const double cLon = ufo ? ufo->getLongitude() : 0.0;
    const double cLat = ufo ? ufo->getLatitude() : 0.0;
    const Base* nearest = nullptr;
    double nearestAngle = 0.0;
    if (ufo && _state->_game->getSavedGame())
    {
        for (const Base* base : *_state->_game->getSavedGame()->getBases())
        {
            const double angle = ufo->getDistance(base->getLongitude(), base->getLatitude());
            if (!nearest || angle < nearestAngle)
            {
                nearest = base;
                nearestAngle = angle;
            }
        }
    }
    const double bLon = nearest ? nearest->getLongitude() : cLon;
    const double bLat = nearest ? nearest->getLatitude() : cLat;
    const double deltaLon = wrapLongitudeDelta(cLon, bLon);
    // OpenXcom latitude grows toward the south, opposite to geographic
    // latitude. Convert both endpoints before evaluating the exact initial
    // great-circle bearing so north stays above the radar origin.
    const double bGeoLat = -bLat;
    const double cGeoLat = -cLat;
    const double dirX = std::sin(deltaLon) * std::cos(cGeoLat);
    const double dirY = std::cos(bGeoLat) * std::sin(cGeoLat)
        - std::sin(bGeoLat) * std::cos(cGeoLat) * std::cos(deltaLon);
    const double dirLen = std::sqrt(dirX * dirX + dirY * dirY);
    const double contactRadius = std::min(plot.w, plot.h) * 0.36;
    if (dirLen < 1e-6)
    {
        m.contact = CalypsoContactIntelMarker{centerX, centerY,
            ufo ? std::string(_state->tr(ufo->getRules()->getSize())) : std::string()};
    }
    else
    {
        m.contact = CalypsoContactIntelMarker{
            centerX + (int)std::llround(dirX / dirLen * contactRadius),
            centerY - (int)std::llround(dirY / dirLen * contactRadius),
            ufo ? std::string(_state->tr(ufo->getRules()->getSize())) : std::string()};
    }
    m.base = CalypsoContactIntelMarker{centerX, centerY,
        nearest ? nearest->getName(_state->_game->getLanguage()) : std::string()};
    m.courseWord = ufo ? boardCourseWord(ufo->getDirection()) : std::string("NONE");

    // Fact rows: labels are config-owned generated copy (reference == engine);
    // every value maps to a real runtime accessor or calculation.
    std::string altitude = ufo ? ufo->getAltitude() : std::string();
    if (ufo)
    {
        altitude = altitude == "STR_GROUND" ? std::string("STR_GROUNDED") : altitude;
        bool underwater = false;
        for (auto& craftType : m.mod->getCraftsList())
        {
            if (underwater) break;
            underwater = m.mod->getCraft(craftType)->isWaterOnly();
        }
        if (underwater && !_state->_state->getGlobe()->insideLand(ufo->getLongitude(), ufo->getLatitude()))
            altitude = "STR_AIRBORNE";
    }
    const std::string classWord = ufo ? std::string(_state->tr(ufo->getRules()->getSize())) : std::string();
    const std::string courseWord = (ufo && ufo->getStatus() == Ufo::FLYING)
        ? std::string(_state->tr(ufo->getDirection())) : std::string(_state->tr("STR_NONE_UC"));
    const std::string speedWord = ufo
        ? Unicode::formatNumber(ufo->getSpeed()) + " kt" : std::string();
    char distBuffer[48];
    if (nearest && ufo)
    {
        const int km = (int)std::llround(nearestAngle * 6371.0);
        snprintf(distBuffer, sizeof(distBuffer), "%d KM", km);
    }
    else
    {
        distBuffer[0] = '\0';
    }
    const std::string factValues[CalypsoF17UfoDetectedGen::kFactCount] = {
        classWord,
        altitude.empty() ? altitude : std::string(_state->tr(altitude)),
        courseWord,
        speedWord,
        std::string(distBuffer),
    };
    for (int index = 0; index < CalypsoF17UfoDetectedGen::kFactCount; ++index)
    {
        m.facts.push_back({CalypsoF17UfoDetectedGen::kFactLabels[index], factValues[index]});
    }

    auto addButton = [&](TextButton* widget, int index, const std::string& label)
    {
        CalypsoSmallConfirmationButton button{};
        button.widget = widget;
        button.text = label;
        button.rect = proj(CalypsoF17UfoDetectedGen::kButtonRects[layoutIdx][index].rect);
        button.tone = generatedActionTone(CalypsoF17UfoDetectedGen::kButtons[index].tone);
        button.restFill = CalypsoF17UfoDetectedGen::kButtons[index].fill;
        button.restBorder = CalypsoF17UfoDetectedGen::kButtons[index].border;
        button.textColor = CalypsoF17UfoDetectedGen::kButtons[index].text;
        m.buttons.push_back(button);
    };
    // Labels come from the LIVE widgets (incl. the Ctrl CANCEL/IGNORE flip);
    // the centre button owns its own translated label.
    addButton(_state->_btnIntercept, 0,
        _state->_btnIntercept ? _state->_btnIntercept->getText() : std::string());
    addButton(_state->_btnCentre, 1,
        _state->_btnCentre ? _state->_btnCentre->getText() : std::string());
    addButton(_state->_btnCancel, 2,
        _state->_btnCancel ? _state->_btnCancel->getText() : std::string());
    calypsoCollectContactIntelBoard(builder, m, _motion);
}

void CalypsoF17UfoDetectedUi::configure(UfoDetectedState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F17")) { s._hdLayout=false; return; }
    s._hdLayout = true;
    const LayoutVariant layout = chooseLayout();
    s._hdWideLayout = (layout == LayoutVariant::Wide);
    applyLayout(s, layout);
    // Rebase the state's UI-scaling capture onto the board design canvas so
    // widget geometry (and the vanilla blit skip) lives in design pixels,
    // exactly like the F21 command-card family (F21Defense precedent).
    s.recaptureUiScaling(designWidth(layout), designHeight(layout), 1.0f,
        /*subtractVanillaCenter=*/false);
    auto* a = new CalypsoF17UfoDetectedUi(&s, layout);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF17UfoDetectedUi::resize(UfoDetectedState& s) {
    if(!s._hdLayout || !s._hdAdapter) return false;
    const LayoutVariant layout = chooseLayout();
    s._hdAdapter->setLayout(layout);
    s._hdWideLayout = (layout == LayoutVariant::Wide);
    applyLayout(s, layout);
    s.recaptureUiScaling(designWidth(layout), designHeight(layout), 1.0f,
        /*subtractVanillaCenter=*/false);
    return true;
}
} }
#endif
