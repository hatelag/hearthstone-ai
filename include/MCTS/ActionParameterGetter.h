#pragma once

#include "FlowControl/IActionParameterGetter.h"
#include "MCTS/MCTS.h"

namespace mcts
{
	// Concept 'ActionChooser':
	//    static int Get(int exclusive_max);
	class ActionParameterGetter : public FlowControl::IActionParameterGetter
	{
	public:
		ActionParameterGetter(MCTS & mcts) : mcts_(mcts) {}
	
		state::CardRef GetDefender(std::vector<state::CardRef> const& targets) final
		{
			assert(!targets.empty());
			size_t size = targets.size();
			return targets[GetNumber((int)size)];
		}

		// Inclusive min & max
		int GetMinionPutLocation(int min, int max) final
		{
			assert(max >= min);
			return min + GetNumber(max - min + 1);
		}

		state::CardRef GetSpecifiedTarget(
			state::State & state, state::CardRef card_ref,
			std::vector<state::CardRef> const& targets) final
		{
			if (targets.empty()) return state::CardRef();
			size_t size = targets.size();
			return targets[GetNumber((int)size)];
		}

		size_t ChooseOne(std::vector<Cards::CardId> const& cards) final
		{
			assert(!cards.empty());
			size_t size = cards.size();
			return (size_t)GetNumber((int)size);
		}

		int GetNumber(int exclusive_max)
		{
			return mcts_.UserChooseAction(exclusive_max);
		}

	private:
		MCTS & mcts_;
	};
}