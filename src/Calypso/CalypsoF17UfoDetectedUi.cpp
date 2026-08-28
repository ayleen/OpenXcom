#ifdef __EMSCRIPTEN__
#include "CalypsoF17UfoDetectedUi.h"
#include <cmath>
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
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"

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

} // namespace

void CalypsoF17UfoDetectedUi::applyLayout(UfoDetectedState& state)
{
	const bool wide = state._hdWideLayout;
	const auto* layout = CalypsoF17UfoDetectedGen::layoutForDesign(
		wide ? 1280 : 740, wide ? 720 : 360);
	if (!layout)
		CalypsoHdUiOverlay::instance().failHdRoute(
			"F17 UFO contact layout is unavailable");
	auto applyRect = [](auto* widget, const auto& rect)
	{
		if (!widget) return;
		widget->setX(rect.x);
		widget->setY(rect.y);
		widget->setWidth(rect.w);
		widget->setHeight(rect.h);
	};
	applyRect(state._window, layout->window);
	const int layoutIndex = wide ? 0 : 1;
	applyRect(state._btnIntercept,
		CalypsoF17UfoDetectedGen::kButtonRects[layoutIndex][0].rect);
	applyRect(state._btnCentre,
		CalypsoF17UfoDetectedGen::kButtonRects[layoutIndex][1].rect);
	applyRect(state._btnCancel,
		CalypsoF17UfoDetectedGen::kButtonRects[layoutIndex][2].rect);
}
CalypsoF17UfoDetectedUi::~CalypsoF17UfoDetectedUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF17UfoDetectedUi::topState() const { return _state; }

void CalypsoF17UfoDetectedUi::collect(CalypsoHdFrameBuilder& builder) const
{
    if(!_state || !_state->_hdLayout || !_state->_game) return;
    const bool wide = _state->_hdWideLayout;
    const int layoutIndex = wide ? 0 : 1;
    const auto* g = CalypsoF17UfoDetectedGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g) return;
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
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
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = proj(g->window); m.status = proj(g->status);
    m.plotPanel = proj(g->plotPanel); m.plotArea = proj(g->plotArea);
    m.reportPanel = proj(g->reportPanel);
    m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message);
    m.footer = proj(g->footer); m.note = proj(g->note);
    for (int i = 1; i <= 5; ++i)
    {
        m.factRects.push_back(proj(factPart(i, false)));
        m.factRects.push_back(proj(factPart(i, true)));
    }
    m.windowWidget = _state->_window;
    m.titleWidget = _state->_txtUfo;
    m.messageWidget = _state->_txtDetected;
    m.protocolText = CalypsoF17UfoDetectedGen::kProtocol;
    m.noteText = CalypsoF17UfoDetectedGen::kNote;
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF17UfoDetectedGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF17UfoDetectedGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF17UfoDetectedGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF17UfoDetectedGen::kPanelFillBottom;
    m.frameColor = CalypsoF17UfoDetectedGen::kFrame;
    m.protocolColor = CalypsoF17UfoDetectedGen::kProtocolText;
    m.dividerColor = CalypsoF17UfoDetectedGen::kDivider;
    m.footerDotColor = CalypsoF17UfoDetectedGen::kFooterDot;
    m.warningColor = CalypsoF17UfoDetectedGen::kWarning;
    m.plotFrameColor = CalypsoF17UfoDetectedGen::kPlotFrame;
    m.plotGridColor = CalypsoF17UfoDetectedGen::kPlotGrid;
    m.plotContactColor = CalypsoF17UfoDetectedGen::kPlotContact;
    m.plotContactHaloColor = CalypsoF17UfoDetectedGen::kPlotContactHalo;
    m.plotBaseColor = CalypsoF17UfoDetectedGen::kPlotBase;
    m.plotCourseColor = CalypsoF17UfoDetectedGen::kPlotCourse;
    m.factDividerColor = CalypsoF17UfoDetectedGen::kFactDivider;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF17UfoDetectedGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF17UfoDetectedGen::kMotionScaleFrom;

    // Runtime content: the vanilla state and its contact remain authoritative.
    Ufo* ufo = _state->_ufo;
    m.titleText = _state->_txtUfo ? _state->_txtUfo->getText() : std::string();
    m.messageText = _state->_txtDetected ? _state->_txtDetected->getText() : std::string();

    // Plot: project the real contact and nearest-base coordinates onto the
    // plot area (north-up local schematic, wrapped longitude deltas).
    const CalypsoLogicalRect& plot = m.plotArea;
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
    const double midLon = cLon + wrapLongitudeDelta(bLon, cLon) * 0.5;
    const double midLat = (cLat + bLat) * 0.5;
    const double halfLon = std::max(1e-4,
        std::max(std::abs(wrapLongitudeDelta(cLon, midLon)),
                 std::abs(wrapLongitudeDelta(bLon, midLon))));
    const double halfLat = std::max(1e-4,
        std::max(std::abs(cLat - midLat), std::abs(bLat - midLat)));
    const double pxPerRadian = std::min(
        0.64 * plot.w / (2.0 * halfLon),
        0.64 * plot.h / (2.0 * halfLat));
    auto projectLon = [&](double lon)
    {
        return plot.x + plot.w / 2 + (int)std::llround(wrapLongitudeDelta(lon, midLon) * pxPerRadian);
    };
    auto projectLat = [&](double lat)
    {
        return plot.y + plot.h / 2 - (int)std::llround((lat - midLat) * pxPerRadian);
    };
    m.contact = CalypsoContactIntelMarker{projectLon(cLon), projectLat(cLat),
        ufo ? std::string(_state->tr(ufo->getRules()->getSize())) : std::string()};
    m.base = CalypsoContactIntelMarker{projectLon(bLon), projectLat(bLat),
        nearest ? nearest->getName(_state->_game->getLanguage()) : std::string()};
    m.courseWord = ufo ? boardCourseWord(ufo->getDirection()) : std::string("NONE");

    // Fact rows: every value maps to a real accessor or is labeled DERIVED.
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
        snprintf(distBuffer, sizeof(distBuffer), "%d KM %s", km, "\xC2\xB7 DERIVED");
    }
    else
    {
        distBuffer[0] = '\0';
    }
    m.facts = {
        {_state->tr("STR_SIZE_UC"), classWord},
        {_state->tr("STR_ALTITUDE"), altitude.empty() ? altitude : std::string(_state->tr(altitude))},
        {_state->tr("STR_HEADING"), courseWord},
        {_state->tr("STR_SPEED"), speedWord},
        {CalypsoF17UfoDetectedGen::kFactLabels[4], std::string(distBuffer)},
    };

    auto addButton = [&](TextButton* widget, int index, const std::string& label)
    {
        CalypsoSmallConfirmationButton button{};
        button.widget = widget;
        button.text = label;
        button.rect = proj(CalypsoF17UfoDetectedGen::kButtonRects[layoutIndex][index].rect);
        button.tone = CalypsoActionTone::Safe;
        button.restFill = CalypsoF17UfoDetectedGen::kButtons[index].fill;
        button.restBorder = CalypsoF17UfoDetectedGen::kButtons[index].border;
        button.textColor = CalypsoF17UfoDetectedGen::kButtons[index].text;
        m.buttons.push_back(button);
    };
    addButton(_state->_btnIntercept, 0,
        _state->_btnIntercept ? _state->_btnIntercept->getText() : std::string());
    addButton(_state->_btnCentre, 1, _state->tr("STR_CENTER_ON_UFO_TIME_5_SECONDS"));
    addButton(_state->_btnCancel, 2,
        _state->_btnCancel ? _state->_btnCancel->getText() : std::string());
    calypsoCollectContactIntelBoard(builder, m, _motion);
}

void CalypsoF17UfoDetectedUi::configure(UfoDetectedState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod()->isHdUiFamilyEnabled("F17")) { s._hdLayout=false; return; }
    s._hdLayout = true;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    applyLayout(s);
    // Rebase the state's UI-scaling capture onto the board design canvas so
    // widget geometry (and the vanilla blit skip) lives in design pixels,
    // exactly like the F21 command-card family (F21Defense precedent).
    s.recaptureUiScaling(s._hdWideLayout ? 1280 : 740, s._hdWideLayout ? 720 : 360, 1.0f,
        /*subtractVanillaCenter=*/false);
    auto* a = new CalypsoF17UfoDetectedUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF17UfoDetectedUi::resize(UfoDetectedState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    applyLayout(s);
    s.recaptureUiScaling(s._hdWideLayout ? 1280 : 740, s._hdWideLayout ? 720 : 360, 1.0f,
        /*subtractVanillaCenter=*/false);
    return true;
}
} }
#endif
