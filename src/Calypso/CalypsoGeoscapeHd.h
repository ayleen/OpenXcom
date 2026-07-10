#ifdef __EMSCRIPTEN__
#ifndef OPENXCOM_CALYPSOGEOSCAPEHD_H
#define OPENXCOM_CALYPSOGEOSCAPEHD_H

/*
 * Phase 41 (Calypso) — Slice B2: HD Geoscape side panel.
 * Whole file is Emscripten-only — the native desktop build never sees it.
 *
 * GeoscapeState grants `friend struct CalypsoGeoscapeHd;` (Geoscape/GeoscapeState.h,
 * #ifdef __EMSCRIPTEN__) so every private-member touch lives HERE; the ctor and
 * resize() hooks in GeoscapeState.cpp only call applyTtf()/layout().
 *
 * layout() computes an edge-anchored panel scale factor
 * (s = max(1, baseYGeoscape/400), since the generic centered enableUiScaling
 * does not apply to a right-edge-docked column) and repositions/resizes every
 * side-panel widget by multiplying its GeoscapeState ctor design offset by s.
 * It also blits the optional CALYPSO_GEOBORD_HD / CALYPSO_GEOBORD_LINE_HD plate
 * art into the existing _sidebar/_sideLine member surfaces — never a per-frame
 * allocation (WASM heap-churn rule) and never by promoting a vanilla sprite id
 * to hd:true (the BACK01.SCR globe-palette incident).
 */

namespace OpenXcom
{

class GeoscapeState;

struct CalypsoGeoscapeHd
{
	/* TTF-ify the side panel: FONT_HD_HUD on the menu/time buttons and the
	 * side fillers, FONT_HD_NUMBERS on the clock/date/funds digits. Null-font
	 * tolerant (getTTFFont(id,false) returns null when the pack is off). */
	static void applyTtf(GeoscapeState *s);

	/* Edge-anchored panel scale: reposition/resize every side-panel widget,
	 * widen _sideLine, and blit the HD plate art (if registered) into the
	 * existing _sidebar/_sideLine surfaces. Called once from the ctor hook and
	 * again from GeoscapeState::resize. */
	static void layout(GeoscapeState *s);
};

}

#endif
#endif
