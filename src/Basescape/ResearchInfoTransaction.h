#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

namespace OpenXcom
{

/**
 * Tracks scientist ownership while a new research project is previewed.
 * Start and cancel are mutually exclusive one-shot transitions.
 */
class ResearchInfoTransaction
{
public:
	enum class Phase
	{
		Inactive,
		Preview,
		Committed,
		Cancelled
	};

	struct Transition
	{
		bool applied;
		int assigned;
	};

	explicit ResearchInfoTransaction(bool preview = false)
		: _phase(preview ? Phase::Preview : Phase::Inactive)
	{
	}

	bool pending() const
	{
		return _phase == Phase::Preview;
	}

	void setAssigned(int assigned)
	{
		if (pending()) _assigned = assigned;
	}

	int assigned() const
	{
		return _assigned;
	}

	Transition start()
	{
		return transitionTo(Phase::Committed);
	}

	Transition cancel()
	{
		return transitionTo(Phase::Cancelled);
	}

	Phase phase() const
	{
		return _phase;
	}

private:
	Transition transitionTo(Phase next)
	{
		if (!pending()) return {false, 0};
		_phase = next;
		const int assigned = _assigned;
		_assigned = 0;
		return {true, assigned};
	}

	Phase _phase;
	int _assigned = 0;
};

}
