#pragma once

/*
 * Emscripten-only L07 visual-state fixture identities.  These profiles are
 * diagnostic harness inputs; ordinary gameplay and native builds never see
 * them.  The renderer still consumes the resulting actions through its normal
 * collectAction/palette path.
 */
#ifdef __EMSCRIPTEN__

#include <array>

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoHdRdVisualStateProfileSpec
{
	int id;
	const char *name;
};

inline const std::array<CalypsoHdRdVisualStateProfileSpec, 8>&
calypsoHdRdVisualStateProfileSpecs()
{
	static const std::array<CalypsoHdRdVisualStateProfileSpec, 8> specs = {{
		{324, "default"}, {325, "hover"}, {326, "focus"}, {327, "selected"},
		{328, "disabled"}, {329, "primary"}, {330, "danger"}, {331, "warning"},
	}};
	return specs;
}

inline const CalypsoHdRdVisualStateProfileSpec*
calypsoHdRdVisualStateProfileSpec(int profile)
{
	for (const auto &spec : calypsoHdRdVisualStateProfileSpecs())
		if (spec.id == profile) return &spec;
	return nullptr;
}

inline bool calypsoHdRdVisualStateProfileValid(int profile)
{
	return calypsoHdRdVisualStateProfileSpec(profile) != nullptr;
}

} // namespace Calypso
} // namespace OpenXcom

#endif // __EMSCRIPTEN__
