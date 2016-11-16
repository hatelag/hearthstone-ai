#pragma once

#include "FlowControl/ActionTypes.h"
#include "State/State.h"
#include "FlowControl/Helpers/PlayCard.h"
#include "FlowControl/Result.h"

namespace FlowControl
{
	template <class ActionParameterGetter, class RandomGenerator>
	class FlowController
	{
	public:
		FlowController(State::State & state, ActionParameterGetter & action_parameters, RandomGenerator & random)
			: state_(state), action_parameters_(action_parameters), random_(random)
		{
		}

		Result PlayCard()
		{
			Helpers::PlayCard<ActionParameterGetter, RandomGenerator> helper(state_, action_parameters_, random_);
			return helper.Go();
		}

		void EndTurn()
		{

		}

	public:
		State::State & state_;
		ActionParameterGetter & action_parameters_;
		RandomGenerator & random_;
	};
}