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
enum class GeoscapePauseReason : std::uint32_t
{
	User = 1u << 0u,
	MoreDrawer = 1u << 1u,
	SessionMenu = 1u << 2u,
	BlockingPopup = 1u << 3u,
	Dogfight = 1u << 4u,
	System = 1u << 5u
};

enum class GeoscapeActionId : std::size_t
{
	Intercept,
	Bases,
	Graphs,
	Ufopaedia,
	Options,
	FundingOrExtended,
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
	{GeoscapeActionId::Intercept, "intercept", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Bases, "bases", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Graphs, "graphs", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Ufopaedia, "ufopaedia", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::Options, "options", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, true, true, true},
	{GeoscapeActionId::FundingOrExtended, "funding-or-extended", GeoscapeActionDisposition::Replace, GeoscapeActionGate::None, true, true, true},
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
	{GeoscapeActionId::QuickLoad, "quick-load", GeoscapeActionDisposition::Move, GeoscapeActionGate::NonIronman, false, true, true},
	{GeoscapeActionId::Speed5Seconds, "speed-5-seconds", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed1Minute, "speed-1-minute", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed5Minutes, "speed-5-minutes", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed30Minutes, "speed-30-minutes", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed1Hour, "speed-1-hour", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Speed1Day, "speed-1-day", GeoscapeActionDisposition::Preserve, GeoscapeActionGate::None, false, true, true},
	{GeoscapeActionId::Pause, "pause", GeoscapeActionDisposition::Add, GeoscapeActionGate::None, false, true, true}
}};

inline const GeoscapeActionSpec& calypsoGeoscapeAction(GeoscapeActionId id)
{
	return GEOSCAPE_ACTIONS[static_cast<std::size_t>(id)];
}

class GeoscapeTimePolicyState
{
public:
	void select(GeoscapeSpeed speed) { _selected = speed; }
	GeoscapeSpeed selectedSpeed() const { return _selected; }
	void acquire(GeoscapePauseReason reason) { _pauseReasons |= static_cast<std::uint32_t>(reason); }
	void release(GeoscapePauseReason reason) { _pauseReasons &= ~static_cast<std::uint32_t>(reason); }
	bool paused() const { return _pauseReasons != 0u; }

private:
	GeoscapeSpeed _selected = GeoscapeSpeed::FiveSeconds;
	std::uint32_t _pauseReasons = 0u;
};

} // namespace Calypso
} // namespace OpenXcom
