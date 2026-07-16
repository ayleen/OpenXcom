#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * Phase 46.1.4 -- opt-in semantic focus coordinator. Pure/header-only: engine
 * targets are opaque identities and activation is supplied explicitly by each
 * family adapter. InteractiveSurface's legacy focus flag is never consulted.
 */
#include "CalypsoFocusModel.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace OpenXcom
{
namespace Calypso
{

enum class CalypsoFocusCommand
{
	Next,
	Previous,
	Activate
};

/// Pure key-decision input used by State's SDL adapter and native tests.
enum class CalypsoFocusKey
{
	Other,
	Tab,
	Return,
	KeypadEnter,
	Space
};

struct CalypsoFocusKeyDecision
{
	bool recognized = false;
	bool invokeCommand = false;
	CalypsoFocusCommand command = CalypsoFocusCommand::Next;
};

/// Classify focus key-down semantics. Key-up pairing is deliberately tracked
/// by State using the concrete SDL symbol consumed on key-down, because the
/// modifier mask can differ by the time the key is released.
inline CalypsoFocusKeyDecision calypsoClassifyFocusKeyDown(
	CalypsoFocusKey key, bool shift, bool ctrl, bool alt, bool gui, bool repeat)
{
	CalypsoFocusKeyDecision out;
	if (ctrl || alt || gui) return out;
	switch (key)
	{
	case CalypsoFocusKey::Tab:
		out.recognized = true;
		out.invokeCommand = true; // navigation repeat is intentional
		out.command = shift ? CalypsoFocusCommand::Previous : CalypsoFocusCommand::Next;
		break;
	case CalypsoFocusKey::Return:
	case CalypsoFocusKey::KeypadEnter:
	case CalypsoFocusKey::Space:
		out.recognized = true;
		out.invokeCommand = !repeat; // consume repeats, activate only once
		out.command = CalypsoFocusCommand::Activate;
		break;
	case CalypsoFocusKey::Other:
		break;
	}
	return out;
}

struct CalypsoFocusBinding
{
	CalypsoFocusNode node;
	void* target = nullptr;
	std::function<bool()> activate;
};

class CalypsoFocusCoordinator
{
public:
	/// Transactional across model and bindings. Invalid identifiers are rejected
	/// by CalypsoFocusModel; null or repeated opaque targets are rejected here
	/// before the model is touched. Empty binding sets are valid.
	bool rebuild(std::vector<CalypsoFocusBinding> bindings, std::uint64_t generation)
	{
		for (std::size_t i = 0; i < bindings.size(); ++i)
		{
			if (!bindings[i].target) return false;
			for (std::size_t j = 0; j < i; ++j)
				if (bindings[i].target == bindings[j].target) return false;
		}

		std::vector<CalypsoFocusNode> nodes;
		nodes.reserve(bindings.size());
		for (const CalypsoFocusBinding& binding : bindings)
			nodes.push_back(binding.node);
		if (!_model.rebuild(std::move(nodes), generation))
			return false;

		_bindings.swap(bindings);
		if (_modalTarget)
			focusModalTarget();
		return true;
	}

	bool restore(const std::string& id, std::uint64_t expectedGeneration)
	{
		if (_modalTarget)
		{
			if (expectedGeneration != _model.generation()) return false;
			focusModalTarget();
			return false;
		}
		return _model.restore(id, expectedGeneration);
	}

	/// Returns false only for a stale generation. With a current generation the
	/// command is owned by the coordinator even if no node moves or activates;
	/// this prevents fallback legacy hotkeys from firing on an opted-in state.
	bool command(CalypsoFocusCommand command, std::uint64_t expectedGeneration,
	             bool wrap = true)
	{
		if (expectedGeneration != _model.generation()) return false;
		if (_modalTarget)
		{
			CalypsoFocusBinding* modal = focusModalTarget();
			if (command == CalypsoFocusCommand::Activate && modal)
			{
				auto activate = modal->activate;
				if (activate) (void)activate();
			}
			return true; // unregistered modal blocks background commands too
		}

		if (command == CalypsoFocusCommand::Next)
			(void)_model.move(CalypsoFocusDirection::Forward, wrap, expectedGeneration);
		else if (command == CalypsoFocusCommand::Previous)
			(void)_model.move(CalypsoFocusDirection::Backward, wrap, expectedGeneration);
		else
		{
			CalypsoFocusBinding* binding = focusedBinding();
			if (binding)
			{
				auto activate = binding->activate;
				if (activate) (void)activate();
			}
		}
		return true;
	}

	/// First modal entry captures the pre-modal stable id once. Modal swaps keep
	/// that origin. Closing restores it only if it is still eligible in the
	/// current generation. An unregistered modal clears semantic focus.
	void modalChanged(void* modalTarget)
	{
		if (modalTarget == _modalTarget) return;
		if (!_modalTarget && modalTarget)
		{
			_preModalCaptured = true;
			_preModalHadFocus = _model.hasFocus();
			_preModalId = _preModalHadFocus ? *_model.focusedId() : std::string();
		}

		if (modalTarget)
		{
			_modalTarget = modalTarget;
			focusModalTarget();
			return;
		}

		_modalTarget = nullptr;
		if (_preModalCaptured && _preModalHadFocus)
			(void)_model.restore(_preModalId, _model.generation());
		else
			clearFocus();
		_preModalCaptured = false;
		_preModalHadFocus = false;
		_preModalId.clear();
	}

	std::uint64_t generation() const { return _model.generation(); }
	bool hasFocus() const { return _model.hasFocus(); }
	const std::string* focusedId() const { return _model.focusedId(); }
	void* focusedTarget() const
	{
		const CalypsoFocusBinding* binding = focusedBindingConst();
		return binding ? binding->target : nullptr;
	}
	void* modalTarget() const { return _modalTarget; }

private:
	CalypsoFocusBinding* bindingForTarget(void* target)
	{
		for (CalypsoFocusBinding& binding : _bindings)
			if (binding.target == target) return &binding;
		return nullptr;
	}

	CalypsoFocusBinding* focusedBinding()
	{
		const std::string* id = _model.focusedId();
		if (!id) return nullptr;
		for (CalypsoFocusBinding& binding : _bindings)
			if (binding.node.id == *id) return &binding;
		return nullptr;
	}

	const CalypsoFocusBinding* focusedBindingConst() const
	{
		const std::string* id = _model.focusedId();
		if (!id) return nullptr;
		for (const CalypsoFocusBinding& binding : _bindings)
			if (binding.node.id == *id) return &binding;
		return nullptr;
	}

	CalypsoFocusBinding* focusModalTarget()
	{
		CalypsoFocusBinding* binding = bindingForTarget(_modalTarget);
		if (binding && binding->node.visible && binding->node.enabled)
		{
			(void)_model.restore(binding->node.id, _model.generation());
			return binding;
		}
		clearFocus();
		return nullptr;
	}

	void clearFocus()
	{
		static const std::string noId;
		(void)_model.restore(noId, _model.generation());
	}

	CalypsoFocusModel _model;
	std::vector<CalypsoFocusBinding> _bindings;
	void* _modalTarget = nullptr;
	bool _preModalCaptured = false;
	bool _preModalHadFocus = false;
	std::string _preModalId;
};

} // namespace Calypso
} // namespace OpenXcom
