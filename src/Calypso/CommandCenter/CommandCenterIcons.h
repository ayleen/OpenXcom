#pragma once
/*
 * Command Center -- icon registry (normative spec 2026-08-28 s.23).
 *
 * The spec defines hand-drawn 24x24 stroke icons; this engine realises the
 * same semantic set through the registered Phosphor line-icon face
 * (FONT_HD_ICONS) so every icon shares one consistent style (spec s.82).
 * Codepoints follow the upstream phosphor-icons/web regular face.
 */
#include <string>

namespace OpenXcom
{
namespace Calypso
{
namespace CommandCenter
{

enum class CcIcon
{
	World,
	Bases,
	Operations,
	Analytics,
	Archive,
	Settings,
	Bell,
	ChevronDown,
	Fullscreen,
	Menu,
	Plus,
	Minus,
};

inline char32_t ccIconGlyph(CcIcon icon)
{
	switch (icon)
	{
		case CcIcon::World: return 0xE28C;      // globe-hemisphere-west
		case CcIcon::Bases: return 0xE2C4;      // house-line
		case CcIcon::Operations: return 0xE1D6; // crosshair
		case CcIcon::Analytics: return 0xE154;  // chart-line
		case CcIcon::Archive: return 0xE00C;    // archive
		case CcIcon::Settings: return 0xE270;   // gear
		case CcIcon::Bell: return 0xE0CE;       // bell
		case CcIcon::ChevronDown: return 0xE136;// caret-down
		case CcIcon::Fullscreen: return 0xE1D0; // corners-out
		case CcIcon::Menu: return 0xE2F0;       // list
		case CcIcon::Plus: return 0xE3D4;       // plus
		case CcIcon::Minus: return 0xE32A;      // minus
	}
	return 0;
}

/// UTF-8 encoding for one icon codepoint (Phosphor PUA = 3-byte range).
inline std::string ccIconUtf8(char32_t glyph)
{
	std::string out;
	if (glyph == 0) return out;
	out += static_cast<char>(0xE0 | (glyph >> 12));
	out += static_cast<char>(0x80 | ((glyph >> 6) & 0x3F));
	out += static_cast<char>(0x80 | (glyph & 0x3F));
	return out;
}

} // namespace CommandCenter
} // namespace Calypso
} // namespace OpenXcom
