#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.1.4 (Calypso) -- pure deterministic focus model foundation.
 *
 * This header intentionally has no SDL or engine dependencies and no callers.
 * Family adapters will later translate semantic focus movement/activation into
 * existing state actions without changing the legacy InteractiveSurface focus
 * flag. Rebuild is the only allocating operation; lookup, restore, movement,
 * and all reads operate on storage prepared by rebuild.
 */
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

struct CalypsoFocusNode
{
	std::string id;
	bool visible = true;
	bool enabled = true;
};

enum class CalypsoFocusDirection
{
	Forward,
	Backward
};

class CalypsoFocusModel
{
public:
	static constexpr std::size_t npos = static_cast<std::size_t>(-1);

	/// Replace the node set atomically. An empty node set is valid. A node with
	/// an empty identifier, or multiple nodes with the same identifier, rejects
	/// the entire candidate and preserves the previous nodes, focus, and
	/// generation. A successful rebuild restores focus by stable identifier only
	/// when that node remains eligible; otherwise it clears focus.
	bool rebuild(std::vector<CalypsoFocusNode> nodes, std::uint64_t generation)
	{
		std::unordered_map<std::string, std::size_t> index;
		index.reserve(nodes.size());
		for (std::size_t i = 0; i < nodes.size(); ++i)
		{
			if (nodes[i].id.empty() || !index.emplace(nodes[i].id, i).second)
				return false;
		}

		std::size_t restored = npos;
		if (_focusIndex != npos)
		{
			const std::string& oldId = _nodes[_focusIndex].id;
			auto it = index.find(oldId);
			if (it != index.end() && eligible(nodes[it->second]))
				restored = it->second;
		}

		_nodes.swap(nodes);
		_index.swap(index);
		_focusIndex = restored;
		_generation = generation;
		return true;
	}

	/// Restore focus by stable identifier for the exact layout generation.
	/// A current-generation miss or ineligible node clears focus. A stale
	/// request is ignored and preserves focus.
	bool restore(const std::string& id, std::uint64_t expectedGeneration)
	{
		if (expectedGeneration != _generation)
			return false;
		auto it = _index.find(id);
		if (it == _index.end() || !eligible(_nodes[it->second]))
		{
			_focusIndex = npos;
			return false;
		}
		_focusIndex = it->second;
		return true;
	}

	/// Move in deterministic declaration order. With no current focus, forward
	/// selects the first eligible node and backward selects the last. Movement
	/// skips hidden/disabled nodes and crosses an edge only when wrap is true.
	/// Failure never changes a valid current focus; an empty/all-ineligible model
	/// safely has no focus.
	bool move(CalypsoFocusDirection direction, bool wrap,
	          std::uint64_t expectedGeneration)
	{
		if (expectedGeneration != _generation)
			return false;

		const std::size_t count = _nodes.size();
		if (count == 0)
		{
			_focusIndex = npos;
			return false;
		}

		if (_focusIndex == npos)
		{
			if (direction == CalypsoFocusDirection::Forward)
			{
				for (std::size_t i = 0; i < count; ++i)
					if (eligible(_nodes[i])) { _focusIndex = i; return true; }
			}
			else
			{
				for (std::size_t i = count; i > 0; --i)
					if (eligible(_nodes[i - 1])) { _focusIndex = i - 1; return true; }
			}
			_focusIndex = npos;
			return false;
		}

		std::size_t candidate = _focusIndex;
		for (std::size_t visited = 0; visited < count; ++visited)
		{
			if (direction == CalypsoFocusDirection::Forward)
			{
				if (candidate + 1 < count)
					++candidate;
				else if (wrap)
					candidate = 0;
				else
					return false;
			}
			else
			{
				if (candidate > 0)
					--candidate;
				else if (wrap)
					candidate = count - 1;
				else
					return false;
			}

			if (eligible(_nodes[candidate]))
			{
				if (candidate == _focusIndex)
					return false;
				_focusIndex = candidate;
				return true;
			}
		}
		return false;
	}

	std::uint64_t generation() const { return _generation; }
	std::size_t size() const { return _nodes.size(); }
	bool empty() const { return _nodes.empty(); }
	bool hasFocus() const { return _focusIndex != npos; }
	std::size_t focusIndex() const { return _focusIndex; }

	const CalypsoFocusNode* nodeAt(std::size_t index) const
	{
		return index < _nodes.size() ? &_nodes[index] : nullptr;
	}

	const CalypsoFocusNode* find(const std::string& id) const
	{
		auto it = _index.find(id);
		return it == _index.end() ? nullptr : &_nodes[it->second];
	}

	const CalypsoFocusNode* focusedNode() const
	{
		return _focusIndex == npos ? nullptr : &_nodes[_focusIndex];
	}

	const std::string* focusedId() const
	{
		const CalypsoFocusNode* node = focusedNode();
		return node ? &node->id : nullptr;
	}

private:
	static bool eligible(const CalypsoFocusNode& node)
	{
		return node.visible && node.enabled;
	}

	std::vector<CalypsoFocusNode> _nodes;
	std::unordered_map<std::string, std::size_t> _index;
	std::size_t _focusIndex = npos;
	std::uint64_t _generation = 0;
};

} // namespace Calypso
} // namespace OpenXcom
