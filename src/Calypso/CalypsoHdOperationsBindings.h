#pragma once
#ifdef __EMSCRIPTEN__
#include "CalypsoHdFontSource.h"
#include "CalypsoHdOperationsModel.h"
namespace OpenXcom { namespace Calypso { namespace OperationsBindings {
inline CalypsoHdOperationsAction action(const std::string &id, const std::string &label,
	const CalypsoHdOperationsRect &rect, const void *widget, bool visible = true,
	const std::string &tone = "normal")
{
	CalypsoHdOperationsAction result;
	result.id = id;
	result.label = label;
	result.component = "management-action-group";
	result.slotRole = "action";
	result.coordinateSpace = "logical";
	result.tone = tone;
	result.visible = rect;
	result.hit = rect;
	result.widget = widget;
	result.state.visible = visible;
	return result;
}

inline CalypsoHdOperationsAction detailAction(const std::string &id,
	const std::string &label, const CalypsoHdOperationsRect &rect,
	const void *widget, bool enabled, const std::string &tone = "normal")
{
	// Detail slots are part of the authored Reference geometry even when no
	// row is selected. Keep the slot visible for parity and carry availability
	// as disabled state instead of dropping its bounds from the model.
	auto result = action(id, label, rect, widget, true, tone);
	result.state.disabled = !enabled;
	return result;
}

inline CalypsoHdOperationsSummaryField summary(const std::string &id,
	const std::string &label, const std::string &value,
	const CalypsoHdOperationsRect &rect, const void *widget)
{
	CalypsoHdOperationsSummaryField result;
	result.id = id;
	result.label = label;
	result.value = value;
	result.rect = rect;
	result.widget = widget;
	return result;
}

inline CalypsoHdOperationsMetric metric(const std::string &id, const std::string &label,
	const std::string &value, const CalypsoHdOperationsRect &rect)
{
	CalypsoHdOperationsMetric result;
	result.id = id;
	result.label = label;
	result.value = value;
	result.rect = rect;
	return result;
}

inline void setContractMetadata(CalypsoHdOperationsModel &model,
	const char *presentation, const char *profileId, const char *profileVersion,
	const char *provenance)
{
	model.presentation = presentation ? presentation : "";
	model.profileId = profileId ? profileId : "";
	model.profileVersion = profileVersion ? profileVersion : "";
	model.provenance = provenance ? provenance : "";
	model.readiness.contractReady = !model.presentation.empty()
		&& !model.profileId.empty() && !model.profileVersion.empty()
		&& !model.provenance.empty();
}

inline void setFonts(CalypsoHdOperationsModel &model, const Mod *mod)
{
	model.readiness.uploadsReady = true;
	model.readiness.retryable = true;
	model.readiness.fontsReady =
		mod != nullptr
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_SB", model.headingFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_INTER_R", model.bodyFont)
		&& calypsoHdResolveFontDescriptor(mod, "FONT_CC_PLEX_R", model.monoFont);
}

template <typename GeneratedProfileStyle,
	typename GeneratedTypographyWide, typename GeneratedTypographyCompact>
inline void finish(CalypsoHdOperationsModel &model, const Mod *mod,
	const char *presentation, const char *profileId, const char *profileVersion,
	const char *provenance, const GeneratedProfileStyle &generatedStyle,
	const GeneratedTypographyWide &typographyWide,
	const GeneratedTypographyCompact &typographyCompact, bool wideLayout)
{
	setContractMetadata(model, presentation, profileId, profileVersion, provenance);
	// One shared binder carries the emitted canonical palette (R06).
	calypsoHdOperationsApplyGeneratedStyle(model, generatedStyle);
	calypsoHdOperationsApplyGeneratedTypography(model,
		wideLayout ? typographyWide : typographyCompact);
	setFonts(model, mod);
}

} } }
#endif
