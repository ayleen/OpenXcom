#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace OpenXcom
{
namespace Calypso
{

enum class GeoscapeActionDisposition { Preserve, Move, Replace, Add };
enum class GeoscapeActionGate { None, ExtendedLinks, Debug, NonIronman };
enum class GeoscapeSpeed { FiveSeconds, OneMinute, FiveMinutes, ThirtyMinutes, OneHour, OneDay };
enum class GeoscapePauseReason : std::size_t
{
	User,
	MoreDrawer,
	SessionMenu,
	BlockingPopup,
	Dogfight,
	System,
	Count
};

enum class GeoscapeActionId : std::size_t
{
	Session,
	Intercept,
	Bases,
	Graphs,
	Ufopaedia,
	Options,
	FundingOrExtended,
	Funding,
	TechTree,
	GlobalResearch,
	GlobalProduction,
	GlobalContainment,
	UfoTracker,
	PilotExperience,
	Notes,
	Music,
	Debug,
	QuickSave,
	InstantSave,
	QuickLoad,
	Speed5Seconds,
	Speed1Minute,
	Speed5Minutes,
	Speed30Minutes,
	Speed1Hour,
	Speed1Day,
	Pause,
	Count
};

struct GeoscapeActionSpec
{
	GeoscapeActionId id;
	std::string_view semanticName;
	GeoscapeActionDisposition disposition;
	GeoscapeActionGate gate;
	bool primary;
	bool reachableWide;
	bool reachableCompact;
};

constexpr std::array<GeoscapeActionSpec, static_cast<std::size_t>(GeoscapeActionId::Count)> GEOSCAPE_ACTIONS{{
	{GeoscapeActionId::Session, "session", GeoscapeActionDisposition::Add, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Intercept, "intercept", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Bases, "bases", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Graphs, "graphs", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Ufopaedia, "ufopaedia", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Options, "options", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::FundingOrExtended, "funding-or-extended", GeoscapeActionDisposition::Replace, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Funding, "funding", GeoscapeActionDisposition::Move, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::TechTree, "tech-tree", GeoscapeActionDisposition::Move, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::GlobalResearch, "global-research", GeoscapeActionDisposition::Move, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::GlobalProduction, "global-production", GeoscapeActionDisposition::Move, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::GlobalContainment, "global-containment", GeoscapeActionDisposition::Move, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::UfoTracker, "ufo-tracker", GeoscapeActionDisposition::Move, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::PilotExperience, "pilot-experience", GeoscapeActionDisposition::Move, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::Notes, "notes", GeoscapeActionDisposition::Move, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::Music, "music", GeoscapeActionDisposition::Replace, GeoscapeActionGate::ExtendedLinks, false, true, true},
	{GeoscapeActionId::Debug, "debug", GeoscapeActionDisposition::Move, GeoscapeActionGate::Debug, false, true, true},
	{GeoscapeActionId::QuickSave, "quick-save", GeoscapeActionDisposition::Move, GeoscapeActionGate::NonIronman, false, true, true},
	{GeoscapeActionId::InstantSave, "instant-save", GeoscapeActionDisposition::Move, GeoscapeActionGate::NonIronman, false, true, true},
	{GeoscapeActionId::QuickLoad, "quick-load", GeoscapeActionDisposition::Move, GeoscapeActionGate::NonIronman, false, true, true},
	{GeoscapeActionId::Speed5Seconds, "speed-5-seconds", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed1Minute, "speed-1-minute", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed5Minutes, "speed-5-minutes", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed30Minutes, "speed-30-minutes", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed1Hour, "speed-1-hour", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed1Day, "speed-1-day", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Pause, "pause", GeoscapeActionDisposition::Add, GeoscapeActionGate::None, false, true, true}
}};

struct GeoscapeActionBinding
{
	std::string_view contractActionId;
	GeoscapeActionId nativeAction;
};

// Exact coverage of strategic-command-shell.requiredActionIds. Duplicate
// nativeAction values are explicit aliases, never implicit string rewriting.
constexpr std::array<GeoscapeActionBinding, 27> GEOSCAPE_ACTION_BINDINGS{{
	{ "action.session", GeoscapeActionId::Session },
	{ "action.bases", GeoscapeActionId::Bases },
	{ "action.graphs", GeoscapeActionId::Graphs },
	{ "action.extended", GeoscapeActionId::FundingOrExtended },
	{ "action.intercept", GeoscapeActionId::Intercept },
	{ "action.ufopaedia", GeoscapeActionId::Ufopaedia },
	{ "action.options", GeoscapeActionId::Options },
	{ "drawer.funding", GeoscapeActionId::Funding },
	{ "drawer.tech-tree", GeoscapeActionId::TechTree },
	{ "drawer.global-research", GeoscapeActionId::GlobalResearch },
	{ "drawer.global-production", GeoscapeActionId::GlobalProduction },
	{ "drawer.global-containment", GeoscapeActionId::GlobalContainment },
	{ "drawer.ufo-tracker", GeoscapeActionId::UfoTracker },
	{ "drawer.pilot-experience", GeoscapeActionId::PilotExperience },
	{ "drawer.notes", GeoscapeActionId::Notes },
	{ "drawer.music", GeoscapeActionId::Music },
	{ "drawer.debug", GeoscapeActionId::Debug },
	{ "drawer.quick-save", GeoscapeActionId::QuickSave },
	{ "drawer.instant-save", GeoscapeActionId::InstantSave },
	{ "drawer.quick-load", GeoscapeActionId::QuickLoad },
	{ "time.pause", GeoscapeActionId::Pause },
	{ "time.speed.5sec", GeoscapeActionId::Speed5Seconds },
	{ "time.speed.1min", GeoscapeActionId::Speed1Minute },
	{ "time.speed.5min", GeoscapeActionId::Speed5Minutes },
	{ "time.speed.30min", GeoscapeActionId::Speed30Minutes },
	{ "time.speed.1hour", GeoscapeActionId::Speed1Hour },
	{ "time.speed.1day", GeoscapeActionId::Speed1Day }
}};

struct GeoscapeConditionalRoute
{
	std::string_view labelKey;
	std::string_view handler;
};

constexpr GeoscapeConditionalRoute calypsoGeoscapeFundingOrExtended(bool extendedLinks)
{
	return extendedLinks
		? GeoscapeConditionalRoute{"STR_EXTENDED", "geoscape.openExtended"}
		: GeoscapeConditionalRoute{"STR_FUNDING", "geoscape.openFunding"};
}

inline const GeoscapeActionSpec& calypsoGeoscapeAction(GeoscapeActionId id)
{
	return GEOSCAPE_ACTIONS[static_cast<std::size_t>(id)];
}

class GeoscapeTimePolicyState
{
public:
	void select(GeoscapeSpeed speed) { _selected = speed; }
	GeoscapeSpeed selectedSpeed() const { return _selected; }
	void acquire(GeoscapePauseReason reason)
	{
		auto& owners = _pauseOwners[static_cast<std::size_t>(reason)];
		++owners;
	}
	void release(GeoscapePauseReason reason)
	{
		auto& owners = _pauseOwners[static_cast<std::size_t>(reason)];
		if (owners != 0u) --owners;
	}
	bool paused() const
	{
		for (const auto owners : _pauseOwners)
			if (owners != 0u) return true;
		return false;
	}

private:
	GeoscapeSpeed _selected = GeoscapeSpeed::FiveSeconds;
	std::array<std::size_t, static_cast<std::size_t>(GeoscapePauseReason::Count)> _pauseOwners{};
};

} // namespace Calypso
} // namespace OpenXcom
