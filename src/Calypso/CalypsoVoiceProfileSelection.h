#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace OpenXcom
{

struct CalypsoVoiceProfileDescriptor
{
	std::string id;
	std::string locale;
	std::string baseLocale;
	std::string unitClass;
	std::string gender;
};

/// Repairs an old civilian tactical identity from the already persisted
/// operation locale. Empty operation metadata keeps the historical English
/// fallback, while a saved per-unit locale always wins.
inline std::string calypsoRepairCivilianVoiceLocale(
	const std::string &unitLocale, const std::string &operationLocale)
{
	if (!unitLocale.empty())
	{
		return unitLocale;
	}
	return operationLocale.empty() ? "en" : operationLocale;
}

/**
 * Deterministically selects a compatible profile without depending on map or
 * ruleset iteration order. The persisted ID wins while it remains compatible.
 */
inline std::string calypsoSelectVoiceProfile(
	const std::vector<CalypsoVoiceProfileDescriptor> &profiles,
	const std::string &locale, const std::string &unitClass,
	const std::string &gender, int stableId, const std::string &stored = std::string())
{
	auto collect = [&](const std::string &wantedLocale, bool baseTier)
	{
		std::vector<std::string> candidates;
		for (const auto &profile : profiles)
		{
			const bool localeMatch = baseTier
				? (profile.locale == wantedLocale || profile.baseLocale == wantedLocale)
				: profile.locale == wantedLocale;
			if (localeMatch && profile.unitClass == unitClass && profile.gender == gender)
			{
				candidates.push_back(profile.id);
			}
		}
		std::sort(candidates.begin(), candidates.end());
		candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
		return candidates;
	};
	auto preserveStored = [&](const std::vector<std::string> &candidates)
	{
		return !stored.empty()
			&& std::binary_search(candidates.begin(), candidates.end(), stored);
	};
	auto localeBase = [](const std::string &value)
	{
		const std::string::size_type separator = value.find('-');
		return separator == std::string::npos ? value : value.substr(0, separator);
	};

	std::vector<std::string> candidates = collect(locale, false);
	if (preserveStored(candidates))
	{
		return stored;
	}
	if (candidates.empty())
	{
		candidates = collect(localeBase(locale), true);
		if (preserveStored(candidates))
		{
			return stored;
		}
	}
	if (candidates.empty() && locale != "en")
	{
		candidates = collect("en", false);
		if (preserveStored(candidates))
		{
			return stored;
		}
	}
	if (candidates.empty())
	{
		return std::string();
	}

	const std::uint32_t id = static_cast<std::uint32_t>(stableId);
	return candidates[id % candidates.size()];
}

}
