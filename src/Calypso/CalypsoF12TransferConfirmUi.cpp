#ifdef __EMSCRIPTEN__
#include "CalypsoF12TransferConfirmUi.h"
#include <string>
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Basescape/TransferConfirmState.h"
#include "../Savegame/Base.h"
#include "../Mod/Mod.h"
#include "Generated/CalypsoF12TransferConfirm.generated.h"
#include "CalypsoHdUiOverlay.h"
#include "CalypsoSmallConfirmationRenderer.h"
#include "CalypsoUiFamilies.h"
namespace OpenXcom { namespace Calypso {
CalypsoF12TransferConfirmUi::~CalypsoF12TransferConfirmUi() { CalypsoHdUiOverlay::instance().clearAdapter(this); }
const void* CalypsoF12TransferConfirmUi::topState() const { return _state; }
const void* CalypsoF12TransferConfirmUi::physicalUnderlayState() const
{
    // The confirmation is a _screen=false popup over the live transfer-items
    // list: compose the HD-owned items below instead of black. An unregistered
    // (non-HD) underlay fails the route closed in the overlay chain walk.
    if (!_state) return nullptr;
    return _state->_state;
}
void CalypsoF12TransferConfirmUi::collectLogicalSuppression(
    CalypsoHdLogicalSuppression& suppression) const
{
    if (!_state) return;
    suppression.add(_state->_window);
    suppression.add(_state->_txtTitle);
    suppression.add(_state->_txtCost);
    suppression.add(_state->_txtTotal);
    suppression.add(_state->_btnCancel);
    suppression.add(_state->_btnOk);
}
namespace
{
/// Strips the native color-flip control byte and ALT markers from a funds
/// value so the HD cost line carries only the readable total. Presentation
/// parsing only, no gameplay.
std::string stripCostValue(const std::string& text)
{
    std::string out;
    for (std::string::size_type i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\x01') continue;
        if (text.compare(i, 5, "{ALT}") == 0) { i += 4; continue; }
        out += text[i];
    }
    const std::string::size_type first = out.find_first_not_of(" \t");
    return first == std::string::npos ? std::string() : out.substr(first);
}
}
void CalypsoF12TransferConfirmUi::collect(CalypsoHdFrameBuilder& builder) const {
    if(!_state || !_state->_hdLayout || !_state->_game)
        CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm prerequisites are unavailable");
    bool wide = _state->_hdWideLayout;
    const auto* g = CalypsoF12TransferConfirmGen::layoutForDesign(wide?1280:740, wide?720:360);
    if(!g)
        CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm generated layout is missing");
    auto winRect = _state->_window ? CalypsoLogicalRect{_state->_window->getX(), _state->_window->getY(), _state->_window->getWidth(), _state->_window->getHeight()} : CalypsoLogicalRect{0,0,1280,720};
    double uiScale = g->window.w ? (double)winRect.w / g->window.w : 1.0;
    auto proj = [&](auto &r){ return CalypsoLogicalRect{ winRect.x + int((r.x - g->window.x)*uiScale), winRect.y + int((r.y - g->window.y)*uiScale), int(r.w*uiScale), int(r.h*uiScale) }; };
    CalypsoSmallConfirmationModel m{};
    m.familyId = CalypsoF12TransferConfirmGen::kFamilyId;
    m.instance = _state; m.mod = _state->_game->getMod(); m.wide = wide;
    m.designWidth = g->designWidth; m.designHeight = g->designHeight;
    m.window = winRect; m.status = proj(g->status); m.warning = proj(g->warning); m.title = proj(g->title); m.message = proj(g->message); m.footer = proj(g->footer);
    m.windowWidget = _state->_window;
    // Approved contract copy with live substitution: the generated title and
    // protocol paint verbatim; the approved message lines carry the live
    // destination base name (for the approved "base" token, byte-safe as the
    // token is ASCII) and the live transfer total. Native texts stay behavior
    // owners via suppression above.
    m.titleWidget = _state->_txtTitle;
    m.titleText = CalypsoF12TransferConfirmGen::kTitle;
    const std::string destination = (_state->_base && !_state->_base->getName().empty())
        ? _state->_base->getName() : std::string();
    if (destination.empty())
        CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm destination is missing");
    m.messageWidget = _state->_txtTotal;
    {
        const std::string label = _state->_txtCost ? _state->_txtCost->getText() : std::string();
        const std::string total = _state->_txtTotal ? stripCostValue(_state->_txtTotal->getText()) : std::string();
        if (total.empty())
            CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm total is missing");
        if (CalypsoF12TransferConfirmGen::kMessageCount <= 0)
            CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm message contract drifted");
        std::string message;
        for (int i = 0; i < CalypsoF12TransferConfirmGen::kMessageCount; ++i)
        {
            if (i > 0) message += " ";
            message += CalypsoF12TransferConfirmGen::kMessage[i];
        }
        for (std::string::size_type pos = 0;
             (pos = message.find("base", pos)) != std::string::npos; pos += destination.size())
            message.replace(pos, 4, destination);
        const std::string cost = label.empty() ? total : (label + ": " + total);
        m.messageText = message + " " + cost;
    }
    m.protocolText = CalypsoF12TransferConfirmGen::kProtocol;
    m.warningGlyph = "!";
    m.cutCornerPx = CalypsoF12TransferConfirmGen::kCutCornerPx;
    m.protocolTextInsetPx = CalypsoF12TransferConfirmGen::kProtocolTextInsetPx;
    m.panelFillTop = CalypsoF12TransferConfirmGen::kPanelFillTop;
    m.panelFillBottom = CalypsoF12TransferConfirmGen::kPanelFillBottom;
    m.frameColor = CalypsoF12TransferConfirmGen::kFrame;
    m.protocolColor = CalypsoF12TransferConfirmGen::kProtocolText;
    m.dividerColor = CalypsoF12TransferConfirmGen::kDivider;
    m.footerDotColor = CalypsoF12TransferConfirmGen::kFooterDot;
    m.warningColor = CalypsoF12TransferConfirmGen::kWarning;
    m.messageDesignWidth = g->message.w;
    m.titleDesignHeight = g->title.h;
    m.motionDurationMs = CalypsoF12TransferConfirmGen::kMotionDurationMs;
    m.motionScaleFrom = CalypsoF12TransferConfirmGen::kMotionScaleFrom;
    struct ButtonBinding { TextButton* widget; const char* id; };
    const ButtonBinding bindings[] = {{_state->_btnCancel, "cancel"}, {_state->_btnOk, "confirm"}};
    const int layoutIdx = (int)(g - CalypsoF12TransferConfirmGen::kLayouts);
    if (layoutIdx < 0 || layoutIdx >= CalypsoF12TransferConfirmGen::kLayoutCount)
        CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm layout contract drifted");
    for (const auto& binding : bindings)
    {
        const CalypsoF12TransferConfirmGen::CalypsoF12TransferConfirmGenButton* generatedButton = nullptr;
        for (int i = 0; i < CalypsoF12TransferConfirmGen::kButtonCount; ++i)
            if (std::string(CalypsoF12TransferConfirmGen::kButtons[i].id) == binding.id)
                { generatedButton = &CalypsoF12TransferConfirmGen::kButtons[i]; break; }
        if (!generatedButton)
            CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm button contract drifted");
        CalypsoSmallConfirmationButton button{};
        button.widget = binding.widget;
        button.text = generatedButton->label;
        button.rect = CalypsoLogicalRect{0, 0, 0, 0};
        for (int i = 0; i < CalypsoF12TransferConfirmGen::kButtonCount; ++i)
            if (std::string(CalypsoF12TransferConfirmGen::kButtonRects[layoutIdx][i].id) == binding.id)
                { button.rect = proj(CalypsoF12TransferConfirmGen::kButtonRects[layoutIdx][i].rect); break; }
        if (button.rect.w <= 0 || button.rect.h <= 0)
            CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm button geometry is missing");
        button.tone = std::string(generatedButton->tone) == "primary"
            ? CalypsoActionTone::Primary
            : (std::string(generatedButton->tone) == "warning"
                ? CalypsoActionTone::Destructive : CalypsoActionTone::Safe);
        button.restFill = generatedButton->fill;
        button.restBorder = generatedButton->border;
        button.textColor = generatedButton->text;
        m.buttons.push_back(button);
    }
    calypsoCollectSmallConfirmation(builder, m, _motion);
}
void CalypsoF12TransferConfirmUi::configure(TransferConfirmState& s, bool allow) {
    if(!allow || !s._game || !s._game->getMod() || !s._game->getLanguage())
        CalypsoHdUiOverlay::instance().failHdRoute("F12 confirm prerequisites are unavailable");
    if (!s._game->getMod()->isHdUiFamilyEnabled("F12")) { s._hdLayout=false; return; }
    s._hdLayout = true; s._hdWideLayout = (Options::baseXResolution >= 1000);
    // Canonical content-sized window: sync vanilla Window to generated rect for 1:1 projection
    { bool wide = s._hdWideLayout; const auto* g = CalypsoF12TransferConfirmGen::layoutForDesign(wide?1280:740, wide?720:360); if (g) { s._window->setX(g->window.x); s._window->setY(g->window.y); s._window->setWidth(g->window.w); s._window->setHeight(g->window.h); } }
    auto* a = new CalypsoF12TransferConfirmUi(&s);
    s._hdAdapter = a;
    CalypsoHdUiOverlay::instance().registerAdapter(a);
}
bool CalypsoF12TransferConfirmUi::resize(TransferConfirmState& s) {
    if(!s._hdLayout) return false;
    s._hdWideLayout = (Options::baseXResolution >= 1000);
    return true;
}
} }
#endif
