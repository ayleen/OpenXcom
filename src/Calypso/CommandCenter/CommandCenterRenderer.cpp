/*
 * Command Center -- renderer (normative spec 2026-08-28).
 * See CommandCenterRenderer.h for the contract.
 */
#ifdef __EMSCRIPTEN__

#include "CommandCenterRenderer.h"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <cmath>

#include "../CalypsoGeoscapeHdShell.h"
#include "../../Geoscape/Globe.h"
#include "../CalypsoGeoscapeColoredLineBatch.h"
#include "../CalypsoGeoscapeHdGlobeDirect.h"
#include "../CalypsoHdFontSource.h"
#include "../../Geoscape/GeoscapeState.h"
#include "../../Engine/Options.h"
#include "../../Interface/TextButton.h"

namespace OpenXcom
{
namespace Calypso
{
namespace CommandCenter
{

namespace
{

using CommandCenterTheme::Color8;

CalypsoLogicalRect ccLogical(const RectF& r)
{
	return { static_cast<int>(std::lround(r.x)), static_cast<int>(std::lround(r.y)),
		static_cast<int>(std::lround(r.width)), static_cast<int>(std::lround(r.height)) };
}

bool g_calypsoCcEnabled = false;

/// The painter consumes integer design rects; Command Center geometry is
/// float (spec s.4), so every draw rounds once at this boundary.
CalypsoF21Rect ccF21(const RectF& r)
{
	return { static_cast<int>(std::lround(r.x)), static_cast<int>(std::lround(r.y)),
		static_cast<int>(std::lround(r.width)), static_cast<int>(std::lround(r.height)) };
}

/// s.11: one panel helper for every Command Center surface.
void ccPanel(CalypsoF21Painter& painter, const RectF& rect, float radius,
	const Color8& top, const Color8& bottom, const Color8& border,
	bool shadow, std::uint32_t& role)
{
	if (shadow)
	{
		CalypsoHdPanelStyle farShadow;
		farShadow.styled = true;
		farShadow.radiusPx = radius + 8.0f;
		farShadow.fillTopRgba = farShadow.fillBottomRgba = 0x0000001Eu;
		RectF far = inflate(rect, 8.0f);
		far.y += 8.0f;
		painter.styled(painter.project(ccF21(far)), farShadow, nullptr, role++);
	}
	CalypsoHdPanelStyle panel;
	panel.styled = true;
	panel.radiusPx = radius;
	panel.borderWidthPx = 1.0f;
	panel.borderColorRgba = CommandCenterTheme::packed(border);
	panel.fillTopRgba = CommandCenterTheme::packed(top);
	panel.fillBottomRgba = CommandCenterTheme::packed(bottom);
	panel.gradDirX = 0.0f;
	panel.gradDirY = 1.0f;
	painter.styled(painter.project(ccF21(rect)), panel, nullptr, role++);
	// Very weak top highlight (s.11 step 5).
	painter.decoration(ccLogical({rect.x + radius, rect.y + 1.0f,
		std::max(1.0f, rect.width - radius * 2.0f), 1.0f}), 0xFFFFFF06u, role++);
}

void ccIcon(CalypsoF21Painter& painter, const RectF& rect, CcIcon icon,
	const CommandCenterFonts& fonts, std::uint32_t color, float sizePx,
	std::uint32_t& role, const void* widget = nullptr)
{
	if (!fonts.ready) return;
	painter.textRect(painter.project(ccF21(rect)), widget, fonts.icons,
		ccIconUtf8(ccIconGlyph(icon)), color, CalypsoHdHAlign::Center,
		CalypsoHdVAlign::Middle, 1, role++, 0.0, sizePx);
}

void ccText(CalypsoF21Painter& painter, const RectF& rect,
	const CalypsoTtfSourceDescriptor& font, const std::string& text,
	std::uint32_t color, float sizePx, float trackingEm,
	CalypsoHdHAlign align, std::uint32_t& role, const void* widget = nullptr)
{
	painter.textRect(painter.project(ccF21(rect)), widget, font, text, color,
		align, CalypsoHdVAlign::Middle, 1, role++, trackingEm, sizePx);
}

void ccRect(CalypsoF21Painter& painter, const RectF& rect, const CalypsoHdPanelStyle& style,
	std::uint32_t& role, const void* widget = nullptr)
{
	painter.styled(painter.project(ccF21(rect)), style, widget, role++);
}

/// Bind one live native widget to a Command Center rect: reposition, claim
/// (suppress the native blit), keep the existing handlers reachable.
void ccBind(CalypsoF21Painter& painter, Surface* widget, const RectF& rect,
	std::uint32_t& role)
{
	if (widget == nullptr) return;
	widget->setX(static_cast<int>(rect.x));
	widget->setY(static_cast<int>(rect.y));
	widget->setWidth(static_cast<int>(rect.width));
	widget->setHeight(static_cast<int>(rect.height));
	painter.claim(widget, role++);
}

struct RailItemSpec { const char* label; CcIcon icon; };
constexpr RailItemSpec kRailItems[5] = {
	{ "WORLD", CcIcon::World }, { "BASES", CcIcon::Bases },
	{ "OPERATIONS", CcIcon::Operations }, { "ANALYTICS", CcIcon::Analytics },
	{ "ARCHIVE", CcIcon::Archive },
};
constexpr const char* kTimeSteps[6] =
	{ "5 SECS", "1 MIN", "5 MINS", "30 MINS", "1 HOUR", "1 DAY" };

} // namespace

bool calypsoCcEnabled() { return g_calypsoCcEnabled; }
void calypsoCcSetEnabled(bool on) { g_calypsoCcEnabled = on; }

namespace
{
CcStageRect g_calypsoCcStage;
}

void calypsoCcSetStageRect(const CcStageRect& rect) { g_calypsoCcStage = rect; }
CcStageRect calypsoCcStageRect() { return g_calypsoCcStage; }

CommandCenterFonts calypsoCcResolveFonts(const Mod* mod)
{
	CommandCenterFonts fonts;
	const bool core =
		calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_R", fonts.interR) &&
		calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_M", fonts.interM) &&
		calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_SB", fonts.interSb) &&
		calypsoHdResolveFontDescriptor(mod, "FONT_CC_PLEX_R", fonts.plexR) &&
		calypsoHdResolveFontDescriptor(mod, "FONT_CC_PLEX_M", fonts.plexM) &&
		calypsoHdResolveFontDescriptor(mod, "FONT_CC_PLEX_SB", fonts.plexSb);
	fonts.ready = core;
	// The icon face is optional: the screen fails closed to labels only.
	if (mod != nullptr)
		calypsoHdResolveFontDescriptor(mod, "FONT_HD_ICONS", fonts.icons);
	return fonts;
}

void calypsoCcRender(CalypsoF21Painter& painter, const CommandCenterLayout& layout,
	const CommandCenterSnapshot& snap, const CommandCenterFonts& fonts,
	bool live, GeoscapeState* state, std::uint32_t& role)
{
	using namespace CommandCenterTheme;
	const auto px = CommandCenterTheme::packed;

	// Pass 1: root background (spec s.17). Live mode paints only the margins
	// around the stage — the world pass owns the stage interior (stage 7);
	// fixture mode paints the full gradient. The radial centre glow is
	// deferred (needs a radial-capable primitive).
	{
		CalypsoHdPanelStyle root;
		root.styled = true;
		root.fillTopRgba = px(BgRoot);
		root.fillBottomRgba = 0x03101Eu;
		root.gradDirX = 0.0f;
		root.gradDirY = 1.0f;
		if (!live)
		{
			ccRect(painter, layout.root, root, role);
		}
		else
		{
			const RectF& st = layout.stage;
			const RectF strips[4] = {
				RectF{0, 0, layout.root.width, st.y},
				RectF{0, st.y, st.x, layout.root.height - st.y},
				RectF{st.right(), st.y, layout.root.width - st.right(), layout.root.height - st.y},
				RectF{st.x, st.bottom(), layout.root.width - st.x, layout.root.height - st.bottom()},
			};
			for (auto& strip : strips)
				if (strip.width > 0.0f && strip.height > 0.0f)
					ccRect(painter, strip, root, role);
		}
	}

	// Pass 2: header background + bottom hairline (spec s.18).
	ccPanel(painter, layout.header, 0.0f, Color8{0x05,0x11,0x1E,0xFF},
		Color8{0x03,0x0D,0x18,0xFF}, BgHeader, false, role);
	painter.decoration(ccLogical({layout.header.x, layout.header.bottom() - 1.0f,
		layout.header.width, 1.0f}), px(BorderSoft), role++);

	// Pass 3: navigation rail background + right hairline (spec s.22).
	{
		CalypsoHdPanelStyle rail;
		rail.styled = true;
		rail.fillTopRgba = 0x051220u;
		rail.fillBottomRgba = 0x030D18u;
		rail.gradDirX = 0.0f;
		rail.gradDirY = 1.0f;
		ccRect(painter, layout.navigationRail, rail, role);
		painter.decoration(ccLogical({layout.navigationRail.right() - 1.0f,
			layout.navigationRail.y, 1.0f, layout.navigationRail.height}),
			px(BorderSoft), role++);
	}

	// Pass 4: stage. Fixture mode paints the full surface + fixed dot grid;
	// live keeps the fill transparent because the globe direct pass owns
	// the world region (spec s.24.2).
	if (!live)
	{
		CalypsoHdPanelStyle stageFill;
		stageFill.styled = true;
		stageFill.radiusPx = RadiusLG;
		stageFill.fillTopRgba = px(BgStage);
		stageFill.fillBottomRgba = px(BgStage);
		ccRect(painter, layout.stage, stageFill, role);
		for (float y = layout.stage.y + 1.0f; y < layout.stage.bottom() - 2.0f; y += 48.0f)
			for (float x = layout.stage.x + 1.0f; x < layout.stage.right() - 2.0f; x += 48.0f)
				painter.decoration(ccLogical({x, y, 1.2f, 1.2f}), 0x8FB1C638u, role++);
	}
	{
		CalypsoHdPanelStyle frame;
		frame.styled = true;
		frame.radiusPx = RadiusLG;
		frame.borderWidthPx = 1.0f;
		frame.borderColorRgba = px(BorderSoft);
		if (!live)
			frame.fillTopRgba = frame.fillBottomRgba = 0x00000000u;
		ccRect(painter, layout.stage, frame, role);
	}

	// Pass 8: zoom cluster (spec s.32).
	{
		const RectF& z = layout.zoomControls;
		ccPanel(painter, z, RadiusMD, Color8{0x07,0x16,0x24,0xF5},
			Color8{0x07,0x16,0x24,0xF5}, Border, false, role);
		ccIcon(painter, RectF{z.x, z.y, z.width, 42.0f}, CcIcon::Plus, fonts,
			px(TextSecondary), 18.0f, role);
		painter.decoration(ccLogical({z.x + 4.0f, z.y + 41.5f, z.width - 8.0f, 1.0f}),
			px(BorderSoft), role++);
		ccIcon(painter, RectF{z.x, z.y + 42.0f, z.width, 42.0f}, CcIcon::Minus, fonts,
			px(TextSecondary), 18.0f, role);
	}

	// Pass 9: timeline (spec s.44-48).
	{
		ccPanel(painter, layout.timeline, RadiusMD, Color8{0x08,0x18,0x27,0xFF},
			Color8{0x04,0x10,0x1D,0xFF}, Border, false, role);
		const float pad = 12.0f;
		const float innerHeight = layout.timeline.height - pad * 2.0f;

		// Play/pause: 56 circle inside a 64 halo (spec s.45).
		const RectF playCol{ layout.timeline.x + pad, layout.timeline.y + pad, 64.0f, innerHeight };
		const RectF halo{ playCol.x + 4.0f, playCol.y + (innerHeight - 64.0f) / 2.0f, 64.0f, 64.0f };
		{
			CalypsoHdPanelStyle ring;
			ring.styled = true;
			ring.radiusPx = 32.0f;
			ring.fillTopRgba = ring.fillBottomRgba = 0x00000000u;
			ring.glowRgba = 0x81E0B509u;
			ring.glowRadiusPx = 4.0f;
			ccRect(painter, halo, ring, role);
		}
		const RectF play{ halo.x + 4.0f, halo.y + 4.0f, 56.0f, 56.0f };
		{
			CalypsoHdPanelStyle circle;
			circle.styled = true;
			circle.radiusPx = 28.0f;
			circle.fillTopRgba = 0x81E0B521u;
			circle.fillBottomRgba = 0x81E0B50Du;
			circle.gradDirX = 0.0f;
			circle.gradDirY = 1.0f;
			circle.borderWidthPx = 1.0f;
			circle.borderColorRgba = px(Accent);
			ccRect(painter, play, circle, role);
		}
		painter.decoration(ccLogical({play.x + play.width / 2.0f - 6.0f, play.y + 19.0f, 3.0f, 18.0f}), px(TextPrimary), role++);
		painter.decoration(ccLogical({play.x + play.width / 2.0f + 3.0f, play.y + 19.0f, 3.0f, 18.0f}), px(TextPrimary), role++);

		// Time step selector (spec s.46).
		const float stepsX = playCol.right() + 12.0f;
		const float fullX = layout.timeline.right() - pad - 40.0f - 12.0f;
		const RectF steps{ stepsX, layout.timeline.y + pad, std::max(0.0f, fullX - stepsX), 42.0f };
		{
			CalypsoHdPanelStyle strip;
			strip.styled = true;
			strip.radiusPx = RadiusSM;
			strip.borderWidthPx = 1.0f;
			strip.borderColorRgba = px(BorderSoft);
			strip.fillTopRgba = strip.fillBottomRgba = 0x00000000u;
			ccRect(painter, steps, strip, role);
		}
		const float segW = steps.width / 6.0f;
		for (int i = 0; i < 6; ++i)
		{
			const RectF seg{ steps.x + segW * i, steps.y, segW, steps.height };
			const bool active = i == snap.selectedTimeStep;
			if (i > 0)
				painter.decoration(ccLogical({seg.x, seg.y + 6.0f, 1.0f, seg.height - 12.0f}), px(BorderSoft), role++);
			ccText(painter, RectF{seg.x, seg.y, seg.width, seg.height - 4.0f}, fonts.plexM,
				kTimeSteps[i], active ? px(Accent) : px(TextMuted), 9.0f, 0.06f,
				CalypsoHdHAlign::Center, role);
			if (active)
				painter.decoration(ccLogical({seg.x + 10.0f, seg.bottom() - 3.0f, seg.width - 20.0f, 2.0f}), px(Accent), role++);
		}

		// Ruler (spec s.47).
		const float rulerY = steps.bottom() + 6.0f;
		const float rulerH = std::min(34.0f, layout.timeline.bottom() - pad - rulerY);
		if (rulerH > 12.0f)
		{
			const RectF ruler{ steps.x, rulerY, steps.width, rulerH };
			painter.decoration(ccLogical({ruler.x, ruler.y + ruler.height / 2.0f, ruler.width, 1.0f}), px(BorderStrong), role++);
			const float stepW = ruler.width / 6.0f;
			for (int i = 0; i <= 6; ++i)
			{
				const float x = ruler.x + stepW * i;
				painter.decoration(ccLogical({x, ruler.y + ruler.height / 2.0f - 4.0f, 1.0f, 8.0f}), px(BorderStrong), role++);
				if (i < 6)
					for (int m = 1; m <= 4; ++m)
						painter.decoration(ccLogical({x + stepW * m / 5.0f, ruler.y + ruler.height / 2.0f - 2.0f, 1.0f, 4.0f}), px(BorderStrong), role++);
			}
			const float markerX = ruler.x + ruler.width * 0.085f;
			CalypsoHdPanelStyle haloDot;
			haloDot.styled = true;
			haloDot.radiusPx = 8.0f;
			haloDot.fillTopRgba = haloDot.fillBottomRgba = 0x00000000u;
			haloDot.glowRgba = 0x81E0B514u;
			haloDot.glowRadiusPx = 4.0f;
			ccRect(painter, RectF{markerX - 8.0f, ruler.y + ruler.height / 2.0f - 8.0f, 16.0f, 16.0f}, haloDot, role);
			CalypsoHdPanelStyle dot;
			dot.styled = true;
			dot.radiusPx = 4.0f;
			dot.fillTopRgba = dot.fillBottomRgba = px(Accent);
			ccRect(painter, RectF{markerX - 4.0f, ruler.y + ruler.height / 2.0f - 4.0f, 8.0f, 8.0f}, dot, role);
		}

		// Fullscreen ghost (spec s.48).
		ccIcon(painter, RectF{layout.timeline.right() - pad - 40.0f,
			layout.timeline.y + pad + (innerHeight - 40.0f) / 2.0f, 40.0f, 40.0f},
			CcIcon::Fullscreen, fonts, px(TextSecondary), 18.0f, role);
	}

	// Pass 12: navigation content (spec s.22).
	{
		const RectF& rail = layout.navigationRail;
		float y = rail.y + Space5;
		for (int i = 0; i < 5; ++i)
		{
			const RectF item{ rail.x + 8.0f, y, rail.width - 16.0f, 72.0f };
			y = item.bottom() + 4.0f;
			const bool active = i == 0; // WORLD is the canonical section (s.61)
			if (active)
			{
				CalypsoHdPanelStyle act;
				act.styled = true;
				act.radiusPx = RadiusSM;
				act.gradDirX = 1.0f;
				act.gradDirY = 0.0f;
				act.fillTopRgba = 0x81E0B51Fu;
				act.fillBottomRgba = 0x81E0B509u;
				act.borderWidthPx = 1.0f;
				act.borderColorRgba = 0x81E0B529u;
				ccRect(painter, item, act, role);
				CalypsoHdPanelStyle glowBar;
				glowBar.styled = true;
				glowBar.radiusPx = 8.0f;
				glowBar.fillTopRgba = glowBar.fillBottomRgba = 0x00000000u;
				glowBar.glowRgba = 0x81E0B514u;
				glowBar.glowRadiusPx = 5.0f;
				ccRect(painter, RectF{item.x - 6.0f, item.y + 13.0f, 13.0f, 46.0f}, glowBar, role);
				painter.decoration(ccLogical({item.x - 1.0f, item.y + 18.0f, 3.0f, 36.0f}), px(Accent), role++);
			}
			ccIcon(painter, RectF{item.x, item.y + 14.0f, item.width, 22.0f}, kRailItems[i].icon,
				fonts, active ? px(Accent) : px(TextSecondary), 22.0f, role);
			ccText(painter, RectF{item.x, item.bottom() - 22.0f, item.width, 14.0f}, fonts.plexM,
				kRailItems[i].label, active ? px(Accent) : px(TextSecondary), 9.0f, 0.08f,
				CalypsoHdHAlign::Center, role);
		}
		// Settings pinned to the rail bottom (spec s.22.6).
		const RectF settings{ rail.x + 24.0f, rail.bottom() - 56.0f, 40.0f, 40.0f };
		CalypsoHdPanelStyle set;
		set.styled = true;
		set.radiusPx = RadiusSM;
		set.borderWidthPx = 1.0f;
		set.borderColorRgba = px(Border);
		set.fillTopRgba = set.fillBottomRgba = 0x00000000u;
		ccRect(painter, settings, set, role);
		ccIcon(painter, settings, CcIcon::Settings, fonts, px(TextMuted), 20.0f, role);
	}

	// Pass 10: inspector (spec s.33-43), reference object content.
	if (layout.inspector.width > 0.0f)
	{
		const RectF& ins = layout.inspector;
		ccPanel(painter, ins, RadiusMD, Color8{0x09,0x19,0x29,0xFF},
			Color8{0x05,0x11,0x1E,0xFF}, Border, true, role);
		const float pad = 16.0f;
		float y = ins.y + pad;
		const float innerW = ins.width - pad * 2.0f;
		ccText(painter, RectF{ins.x + pad, y, innerW, 12.0f}, fonts.plexM,
			"SELECTED OBJECT", px(TextMuted), 9.0f, 0.12f, CalypsoHdHAlign::Left, role);
		y += 24.0f;
		{
			CalypsoHdPanelStyle avatar;
			avatar.styled = true;
			avatar.radiusPx = 20.0f;
			avatar.fillTopRgba = avatar.fillBottomRgba = 0x81E0B50Fu;
			avatar.borderWidthPx = 1.0f;
			avatar.borderColorRgba = 0x81E0B540u;
			ccRect(painter, RectF{ins.x + pad, y, 40.0f, 40.0f}, avatar, role);
			ccIcon(painter, RectF{ins.x + pad, y, 40.0f, 40.0f}, CcIcon::Intercept, fonts, px(TextPrimary), 20.0f, role);
			ccText(painter, RectF{ins.x + pad + 50.0f, y - 2.0f, innerW - 50.0f, 18.0f}, fonts.plexSb,
				"INTERCEPT-07", px(TextPrimary), 12.0f, 0.04f, CalypsoHdHAlign::Left, role);
			ccText(painter, RectF{ins.x + pad + 50.0f, y + 20.0f, innerW - 50.0f, 12.0f}, fonts.plexSb,
				"ACTIVE", px(Accent), 9.0f, 0.08f, CalypsoHdHAlign::Left, role);
		}
		y += 62.0f;
		static const char* kProps[][2] = {
			{ "TYPE", "INTERCEPT" }, { "STATUS", "VECTOR LOCKED" },
			{ "ETA", "00:05:12" }, { "ORIGIN", "NORAD STATION" },
			{ "TARGET", "HOSTILE CONTACT" }, { "COORDINATES", "12.45 N 135.67 W" },
		};
		for (auto& row : kProps)
		{
			ccText(painter, RectF{ins.x + pad, y, innerW * 0.5f, 14.0f}, fonts.plexM,
				row[0], px(TextMuted), 8.0f, 0.08f, CalypsoHdHAlign::Left, role);
			ccText(painter, RectF{ins.x + pad, y, innerW, 14.0f}, fonts.plexR,
				row[1], px(TextSecondary), 9.0f, 0.02f, CalypsoHdHAlign::Right, role);
			y += 22.0f;
		}
		y += 6.0f;
		painter.decoration(ccLogical({ins.x + pad, y, innerW, 1.0f}), px(BorderSoft), role++);
		y += 14.0f;
		ccText(painter, RectF{ins.x + pad, y, innerW, 12.0f}, fonts.plexM,
			"MISSION OVERVIEW", px(TextMuted), 9.0f, 0.12f, CalypsoHdHAlign::Left, role);
		y += 20.0f;
		{
			const RectF mm{ ins.x + pad, y, innerW, 96.0f };
			CalypsoHdPanelStyle mmPanel;
			mmPanel.styled = true;
			mmPanel.radiusPx = RadiusSM;
			mmPanel.fillTopRgba = mmPanel.fillBottomRgba = 0x04101Cu;
			mmPanel.borderWidthPx = 1.0f;
			mmPanel.borderColorRgba = px(BorderSoft);
			ccRect(painter, mm, mmPanel, role);
			for (int i = 1; i <= 3; ++i)
				painter.decoration(ccLogical({mm.x + mm.width * 0.25f * i, mm.y + 2.0f, 1.0f, mm.height - 4.0f}), 0x6FA5B814u, role++);
			painter.decoration(ccLogical({mm.x + 2.0f, mm.y + mm.height * 0.5f, mm.width - 4.0f, 1.0f}), 0x6FA5B814u, role++);
			painter.decoration(ccLogical({mm.x + mm.width * 0.14f, mm.y + mm.height * 0.70f - 1.0f, mm.width * 0.68f, 2.0f}), px(Accent), role++);
			painter.decoration(ccLogical({mm.x + mm.width * 0.14f - 2.0f, mm.y + mm.height * 0.70f - 2.0f, 4.0f, 4.0f}), px(Accent), role++);
			painter.decoration(ccLogical({mm.x + mm.width * 0.82f - 2.0f, mm.y + mm.height * 0.34f - 2.0f, 4.0f, 4.0f}), px(Accent), role++);
			y += 108.0f;
		}
		ccText(painter, RectF{ins.x + pad, y, innerW, 11.0f}, fonts.plexM,
			"OBJECTIVE", px(TextMuted), 8.0f, 0.08f, CalypsoHdHAlign::Left, role);
		y += 13.0f;
		ccText(painter, RectF{ins.x + pad, y, innerW, 15.0f}, fonts.interR,
			"Neutralize hostile contact", px(TextSecondary), 11.0f, 0.0f, CalypsoHdHAlign::Left, role);
		y += 23.0f;
		painter.decoration(ccLogical({ins.x + pad, y + 3.0f, 6.0f, 6.0f}), px(Danger), role++);
		ccText(painter, RectF{ins.x + pad + 12.0f, y, innerW - 12.0f, 12.0f}, fonts.plexM,
			"PRIORITY HIGH", px(TextSecondary), 8.0f, 0.08f, CalypsoHdHAlign::Left, role);
		y += 22.0f;
		// INTERCEPT primary button (spec s.42).
		{
			const RectF btn{ ins.x + pad, y, innerW, 44.0f };
			CalypsoHdPanelStyle shadow;
			shadow.styled = true;
			shadow.radiusPx = 11.0f;
			shadow.fillTopRgba = shadow.fillBottomRgba = 0x42C2891Fu;
			ccRect(painter, RectF{btn.x - 4.0f, btn.y + 4.0f, btn.width + 8.0f, btn.height + 8.0f}, shadow, role);
			CalypsoHdPanelStyle prim;
			prim.styled = true;
			prim.radiusPx = 7.0f;
			prim.fillTopRgba = px(AccentStrong);
			prim.fillBottomRgba = px(Accent);
			prim.gradDirX = 0.0f;
			prim.gradDirY = 1.0f;
			ccRect(painter, btn, prim, role);
			ccText(painter, RectF{btn.x, btn.y, btn.width - 24.0f, btn.height}, fonts.plexSb,
				"INTERCEPT", px(TextOnAccent), 11.0f, 0.10f, CalypsoHdHAlign::Center, role);
			ccIcon(painter, RectF{btn.right() - 32.0f, btn.y, 16.0f, btn.height}, CcIcon::Intercept,
				fonts, px(TextOnAccent), 16.0f, role);
			y += 52.0f;
		}
		// VIEW DETAILS ghost (spec s.43).
		{
			const RectF btn{ ins.x + pad, y, innerW, 36.0f };
			CalypsoHdPanelStyle ghost;
			ghost.styled = true;
			ghost.radiusPx = 7.0f;
			ghost.borderWidthPx = 1.0f;
			ghost.borderColorRgba = px(Border);
			ghost.fillTopRgba = ghost.fillBottomRgba = 0x00000000u;
			ccRect(painter, btn, ghost, role);
			ccText(painter, btn, fonts.plexM, "VIEW DETAILS", px(TextSecondary), 9.0f, 0.10f, CalypsoHdHAlign::Center, role);
		}
	}

	// Pass 11: header content (spec s.19-21).
	{
		// Session selector.
		const RectF& sel = layout.sessionSelector;
		ccPanel(painter, sel, RadiusSM, BgPanel, BgPanel, Border, false, role);
		const RectF avatar{ sel.x + 10.0f, sel.y + 8.0f, 32.0f, 32.0f };
		{
			CalypsoHdPanelStyle ring;
			ring.styled = true;
			ring.radiusPx = 16.0f;
			ring.fillTopRgba = ring.fillBottomRgba = px(AccentSoft);
			ring.borderWidthPx = 1.0f;
			ring.borderColorRgba = 0x81E0B54Du;
			ccRect(painter, avatar, ring, role);
			ccIcon(painter, avatar, CcIcon::World, fonts, px(Accent), 19.0f, role);
		}
		ccText(painter, RectF{avatar.right() + 10.0f, sel.y + 9.0f, 120.0f, 11.0f}, fonts.plexM,
			snap.sessionCaption, px(TextMuted), 9.0f, 0.12f, CalypsoHdHAlign::Left, role);
		ccText(painter, RectF{avatar.right() + 10.0f, sel.y + 22.0f, 120.0f, 16.0f}, fonts.interSb,
			snap.sessionName, px(TextPrimary), 12.0f, 0.04f, CalypsoHdHAlign::Left, role);
		ccIcon(painter, RectF{sel.right() - 26.0f, sel.y, 16.0f, sel.height}, CcIcon::ChevronDown,
			fonts, px(TextSecondary), 14.0f, role);

		// Right group: date/time, divider, system status, bell (spec s.20-21).
		const float rightGroupX = layout.header.width - 16.0f - 329.0f;
		const RectF& dt = layout.dateTimeBlock;
		ccText(painter, RectF{dt.x, dt.y, dt.width, 14.0f}, fonts.plexM,
			snap.displayTime, px(TextPrimary), 10.0f, 0.08f, CalypsoHdHAlign::Right, role);
		ccText(painter, RectF{dt.x, dt.y + 18.0f, dt.width, 14.0f}, fonts.plexM,
			snap.displayDate, px(TextSecondary), 10.0f, 0.08f, CalypsoHdHAlign::Right, role);
		painter.decoration(ccLogical({rightGroupX + 113.0f, layout.header.y + (layout.header.height - 32.0f) / 2.0f, 1.0f, 32.0f}), px(Border), role++);
		const RectF& st = layout.systemStatusBlock;
		ccText(painter, RectF{st.x, st.y, st.width, 11.0f}, fonts.plexM,
			"SYSTEM STATUS", px(TextMuted), 8.0f, 0.12f, CalypsoHdHAlign::Left, role);
		painter.decoration(ccLogical({st.x, st.y + 17.0f, 6.0f, 6.0f}),
			snap.systemNominal ? px(Success) : px(Danger), role++);
		ccText(painter, RectF{st.x + 12.0f, st.y + 12.0f, st.width - 12.0f, 14.0f}, fonts.plexSb,
			snap.systemStatus, px(TextSecondary), 10.0f, 0.08f, CalypsoHdHAlign::Left, role);
		ccIcon(painter, layout.notificationButton, CcIcon::Bell, fonts,
			snap.hasUnreadNotification ? px(TextPrimary) : px(TextSecondary), 18.0f, role);
		if (snap.hasUnreadNotification)
			painter.decoration(ccLogical({layout.notificationButton.right() - 13.0f, layout.notificationButton.y + 7.0f, 6.0f, 6.0f}), px(Accent), role++);
	}

	// Live widget binding: reposition the existing interactive surfaces onto
	// the Command Center grid and claim them so the native blit stays hidden
	// (spec s.76 -- handlers stay, only the call points move).
	if (live && state != nullptr)
	{
		// Stage 7: publish the stage rect in GLOBE-WIDGET logical pixels.
		// The direct pass multiplies by the screen scales exactly like its
		// own u_globeCenter math, so the globe disk, routes and scissor land
		// inside the stage regardless of DPR or canvas fitting.
		if (const Surface* globe = CalypsoGeoscapeHdShell::resolveWidget(state, "bg"))
		{
			const double fx = globe->getWidth() > 0
				? static_cast<double>(globe->getWidth()) / 1280.0 : 1.0;
			const double fy = globe->getHeight() > 0
				? static_cast<double>(globe->getHeight()) / 720.0 : 1.0;
			CcStageRect sr;
			sr.active = true;
			sr.x = (int)std::llround(layout.stage.x * fx);
			sr.y = (int)std::llround(layout.stage.y * fy);
			sr.w = (int)std::llround(layout.stage.width * fx);
			sr.h = (int)std::llround(layout.stage.height * fy);
			calypsoCcSetStageRect(sr);
		}
		Surface* session = const_cast<Surface*>(CalypsoGeoscapeHdShell::resolveLiveWidget(state, "action.session"));
		Surface* pause = const_cast<Surface*>(CalypsoGeoscapeHdShell::resolveLiveWidget(state, "time.pause"));
		ccBind(painter, session, layout.sessionSelector, role);
		ccBind(painter, pause, RectF{layout.timeline.x + 20.0f,
			layout.timeline.y + 12.0f + (layout.timeline.height - 24.0f - 56.0f) / 2.0f, 56.0f, 56.0f}, role);
		// Rail items: provisional action mapping (spec s.76) until the
		// backend stage introduces the section router.
		ccBind(painter, CalypsoGeoscapeHdShell::resolveWidget(state, "btnBases"), RectF{layout.navigationRail.x + 8.0f, layout.navigationRail.y + Space5 + 76.0f, layout.navigationRail.width - 16.0f, 72.0f}, role);
		ccBind(painter, CalypsoGeoscapeHdShell::resolveWidget(state, "btnGraphs"), RectF{layout.navigationRail.x + 8.0f, layout.navigationRail.y + Space5 + 152.0f, layout.navigationRail.width - 16.0f, 72.0f}, role);
		ccBind(painter, CalypsoGeoscapeHdShell::resolveWidget(state, "btnIntercept"), RectF{layout.navigationRail.x + 8.0f, layout.navigationRail.y + Space5 + 228.0f, layout.navigationRail.width - 16.0f, 72.0f}, role);
		ccBind(painter, CalypsoGeoscapeHdShell::resolveWidget(state, "btnUfopaedia"), RectF{layout.navigationRail.x + 8.0f, layout.navigationRail.y + Space5 + 304.0f, layout.navigationRail.width - 16.0f, 72.0f}, role);
		ccBind(painter, CalypsoGeoscapeHdShell::resolveWidget(state, "btnOptions"), RectF{layout.navigationRail.x + 24.0f, layout.navigationRail.bottom() - 56.0f, 40.0f, 40.0f}, role);
		// Time steps: the six native buttons map onto the selector segments.
		const float stepsX = layout.timeline.x + 12.0f + 64.0f + 12.0f;
		const float stepsW = layout.timeline.right() - 12.0f - 40.0f - 12.0f - stepsX;
		Surface* speed[6] = { CalypsoGeoscapeHdShell::resolveWidget(state, "btn5Secs"), CalypsoGeoscapeHdShell::resolveWidget(state, "btn1Min"), CalypsoGeoscapeHdShell::resolveWidget(state, "btn5Mins"),
			CalypsoGeoscapeHdShell::resolveWidget(state, "btn30Mins"), CalypsoGeoscapeHdShell::resolveWidget(state, "btn1Hour"), CalypsoGeoscapeHdShell::resolveWidget(state, "btn1Day") };
		for (int i = 0; i < 6; ++i)
			ccBind(painter, speed[i], RectF{stepsX + stepsW / 6.0f * i, layout.timeline.y + 12.0f, stepsW / 6.0f, 42.0f}, role);
		// Zoom cluster buttons.
		ccBind(painter, CalypsoGeoscapeHdShell::resolveWidget(state, "btnZoomIn"), RectF{layout.zoomControls.x, layout.zoomControls.y, 40.0f, 42.0f}, role);
		ccBind(painter, CalypsoGeoscapeHdShell::resolveWidget(state, "btnZoomOut"), RectF{layout.zoomControls.x, layout.zoomControls.y + 42.0f, 40.0f, 42.0f}, role);
		// Header texts are re-drawn by CC; claim the native ones away.
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtHour"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtHourSep"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtMin"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtMinSep"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtSec"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtWeekday"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtDay"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtMonth"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtYear"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "txtFunds"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "sidebar"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "sideLine"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "sideTop"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "sideBottom"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "btnRotateLeft"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "btnRotateRight"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "btnRotateUp"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "btnRotateDown"), role++);
		painter.claim(CalypsoGeoscapeHdShell::resolveWidget(state, "btnFunding"), role++);
	}
}

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom

EMSCRIPTEN_KEEPALIVE
extern "C" void calypso_set_command_center(int on)
{
	OpenXcom::Calypso::CommandCenter::calypsoCcSetEnabled(on != 0);
}

#endif /* __EMSCRIPTEN__ */
