#include <assert.h>
#include <iostream>

#include "engine/FlowControl/FlowController.h"
#include "engine/FlowControl/FlowController-impl.h"
#include "engine/FlowControl/ValidActionGetter.h"
#include "engine/ActionTargets.h"
#include "engine/ActionTargets-impl.h"

class Test3_ActionParameterGetter : public engine::FlowControl::IActionParameterGetter
{
public:
	Test3_ActionParameterGetter() :
		targets_(),
		next_defender_count(0),
		next_defender_idx(0),
		next_minion_put_location(0),
		next_specified_target_count(0),
		next_specified_target_idx(0)
	{}

	void SetMainOp(engine::MainOpType main_op) { main_op_ = main_op; }
	engine::MainOpType ChooseMainOp() { return main_op_; }

	void AnalyzeTargets(state::State const& game) {
		targets_.Analyze(engine::FlowControl::ValidActionGetter(game));
	}

	void SetHandCard(int hand_card) { hand_card_ = hand_card; }
	int ChooseHandCard() { return hand_card_; }

	void SetAttacker(state::CardRef attacker) { attacker_ = attacker; }
	state::CardRef GetAttacker() override { return attacker_; }

	state::CardRef GetDefender(std::vector<int> const& targets) override
	{
		if (next_defender_count >= 0) assert(next_defender_count == (int)targets.size());
		assert(next_defender_idx >= 0);
		assert(next_defender_idx < (int)targets.size());
		int target_idx = targets[next_defender_idx];
		return targets_.GetCardRef(target_idx);
	}

	int GetMinionPutLocation(int minions) override
	{
		return next_minion_put_location;
	}

	state::CardRef GetSpecifiedTarget(state::State & state, state::CardRef card_ref,std::vector<state::CardRef> const& targets) override
	{
		assert((int)targets.size() == next_specified_target_count);

		if (next_specified_target_idx < 0) return state::CardRef();
		else return targets[next_specified_target_idx];
	}

	Cards::CardId ChooseOne(std::vector<Cards::CardId> const& cards) override {
		assert(false);
		return cards[0];
	}

	engine::MainOpType main_op_;
	engine::ActionTargets targets_;

	int hand_card_;
	state::CardRef attacker_;

	int next_defender_count;
	int next_defender_idx;

	int next_minion_put_location;

	int next_specified_target_count;
	int next_specified_target_idx;
};

class Test3_RandomGenerator : public engine::FlowControl::IRandomGenerator
{
public:
	Test3_RandomGenerator() :called_times(0), next_rand(0) {}

	int Get(int exclusive_max)
	{
		++called_times;
		return next_rand;
	}

	size_t Get(size_t exclusive_max) { return (size_t)Get((int)exclusive_max); }

	int Get(int min, int max)
	{
		++called_times;
		return min + next_rand;
	}

public:
	int called_times;
	int next_rand;
};

static void PushBackDeckCard(Cards::CardId id, engine::FlowControl::FlowContext & flow_context, state::State & state, state::PlayerIdentifier player)
{
	int deck_count = (int)state.GetBoard().Get(player).deck_.Size();

	((Test3_RandomGenerator&)(flow_context.GetRandom())).next_rand = deck_count;
	((Test3_RandomGenerator&)(flow_context.GetRandom())).called_times = 0;

	state.GetBoard().Get(player).deck_.ShuffleAdd(id, [&](int exclusive_max) {
		return flow_context.GetRandom().Get(exclusive_max);
	});

	if (deck_count > 0) assert(((Test3_RandomGenerator&)(flow_context.GetRandom())).called_times > 0);
	++deck_count;

	assert(state.GetBoard().Get(player).deck_.Size() == deck_count);
}

static void MakeDeck(state::State & state, engine::FlowControl::FlowContext & flow_context, state::PlayerIdentifier player)
{
	PushBackDeckCard(Cards::ID_EX1_020, flow_context, state, player);
	PushBackDeckCard(Cards::ID_EX1_020, flow_context, state, player);
	PushBackDeckCard(Cards::ID_CS2_171, flow_context, state, player);
}

static state::Cards::Card CreateHandCard(Cards::CardId id, state::State & state, state::PlayerIdentifier player)
{
	state::Cards::CardData raw_card = Cards::CardDispatcher::CreateInstance(id);

	raw_card.enchanted_states.player = player;
	raw_card.zone = state::kCardZoneNewlyCreated;
	raw_card.enchantment_handler.SetOriginalStates(raw_card.enchanted_states);

	return state::Cards::Card(raw_card);
}

static state::CardRef AddHandCard(Cards::CardId id, engine::FlowControl::FlowContext & flow_context, state::State & state, state::PlayerIdentifier player)
{
	int hand_count = (int)state.GetBoard().Get(player).hand_.Size();

	auto ref = state.AddCard(CreateHandCard(id, state, player));
	state.GetZoneChanger<state::kCardZoneNewlyCreated>(ref)
		.ChangeTo<state::kCardZoneHand>(player);

	assert(state.GetCardsManager().Get(ref).GetCardId() == id);
	assert(state.GetCardsManager().Get(ref).GetPlayerIdentifier() == player);
	if (hand_count == 10) {
		assert(state.GetBoard().Get(player).hand_.Size() == 10);
		assert(state.GetCardsManager().Get(ref).GetZone() == state::kCardZoneGraveyard);
	}
	else {
		++hand_count;
		assert((int)state.GetBoard().Get(player).hand_.Size() == hand_count);
		assert(state.GetBoard().Get(player).hand_.Get(hand_count - 1) == ref);
		assert(state.GetCardsManager().Get(ref).GetZone() == state::kCardZoneHand);
		assert(state.GetCardsManager().Get(ref).GetZonePosition() == (hand_count - 1));
	}

	return ref;
}

static void MakeHand(state::State & state, engine::FlowControl::FlowContext & flow_context, state::PlayerIdentifier player)
{
	AddHandCard(Cards::ID_CS2_141, flow_context, state, player);
}

static void MakeHero(state::State & state, engine::FlowControl::FlowContext & flow_context, state::PlayerIdentifier player)
{
	state::Cards::CardData raw_card;
	raw_card.card_id = (Cards::CardId)8;
	raw_card.card_type = state::kCardTypeHero;
	raw_card.zone = state::kCardZoneNewlyCreated;
	raw_card.enchanted_states.max_hp = 30;
	raw_card.enchanted_states.player = player;
	raw_card.enchanted_states.attack = 0;
	raw_card.enchantment_handler.SetOriginalStates(raw_card.enchanted_states);

	state::CardRef ref = state.AddCard(state::Cards::Card(raw_card));

	state.GetZoneChanger<state::kCardTypeHero, state::kCardZoneNewlyCreated>(ref)
		.ChangeTo<state::kCardZonePlay>(player);


	auto hero_power = Cards::CardDispatcher::CreateInstance(Cards::ID_CS1h_001);
	assert(hero_power.card_type == state::kCardTypeHeroPower);
	hero_power.zone = state::kCardZoneNewlyCreated;
	ref = state.AddCard(state::Cards::Card(hero_power));
	state.GetZoneChanger<state::kCardTypeHeroPower, state::kCardZoneNewlyCreated>(ref)
		.ChangeTo<state::kCardZonePlay>(player);
}

struct MinionCheckStats
{
	int attack;
	int hp;
	int max_hp;
};

static void CheckMinion(state::State &state, state::CardRef ref, MinionCheckStats const& stats)
{
	assert(state.GetCardsManager().Get(ref).GetAttack() == stats.attack);
	assert(state.GetCardsManager().Get(ref).GetMaxHP() == stats.max_hp);
	assert(state.GetCardsManager().Get(ref).GetHP() == stats.hp);
}

static void CheckMinions(state::State & state, state::PlayerIdentifier player, std::vector<MinionCheckStats> const& checking)
{
	std::vector<state::CardRef> const& minions = state.GetBoard().Get(player).minions_.Get();

	assert(minions.size() == checking.size());
	for (size_t i = 0; i < minions.size(); ++i) {
		CheckMinion(state, minions[i], checking[i]);
	}
}

struct CrystalCheckStats
{
	int current;
	int total;
};
static void CheckCrystals(state::State & state, state::PlayerIdentifier player, CrystalCheckStats checking)
{
	assert(state.GetBoard().Get(player).GetResource().GetCurrent() == checking.current);
	assert(state.GetBoard().Get(player).GetResource().GetTotal() == checking.total);
}

static void CheckHero(state::State & state, state::PlayerIdentifier player, int hp, int armor, int attack)
{
	auto hero_ref = state.GetBoard().Get(player).GetHeroRef();
	auto const& hero = state.GetCardsManager().Get(hero_ref);
	(void)hero;

	assert(hero.GetHP() == hp);
	assert(hero.GetArmor() == armor);
	assert(hero.GetAttack() == attack);
}

void test3()
{
	Test3_ActionParameterGetter parameter_getter;
	Test3_RandomGenerator random;
	state::State state;
	engine::FlowControl::FlowContext flow_context(random, parameter_getter);

	engine::FlowControl::FlowController controller(state, flow_context);

	MakeHero(state, flow_context, state::PlayerIdentifier::First());
	MakeDeck(state, flow_context, state::PlayerIdentifier::First());
	MakeHand(state, flow_context, state::PlayerIdentifier::First());

	MakeHero(state, flow_context, state::PlayerIdentifier::Second());
	MakeDeck(state, flow_context, state::PlayerIdentifier::Second());
	MakeHand(state, flow_context, state::PlayerIdentifier::Second());

	state.GetMutableCurrentPlayerId().SetFirst();
	state.GetBoard().GetFirst().GetResource().SetTotal(4);
	state.GetBoard().GetFirst().GetResource().Refill();
	state.GetBoard().GetSecond().GetResource().SetTotal(4);

	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 4, 4 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 4 });
	CheckMinions(state, state::PlayerIdentifier::First(), {});
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

	parameter_getter.next_specified_target_count = 2;
	parameter_getter.next_specified_target_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(0);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 1, 4 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 4 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {2, 2, 2} });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

	PushBackDeckCard(Cards::ID_CS2_122, flow_context, state, state::PlayerIdentifier::Second());

	random.next_rand = 3;
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 1, 4 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(1);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 1, 4 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2} });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

	PushBackDeckCard(Cards::ID_CS2_122, flow_context, state, state::PlayerIdentifier::First());

	random.next_rand = 3;
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(0);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 }, {2, 2, 2} });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

	AddHandCard(Cards::ID_EX1_506, flow_context, state, state::PlayerIdentifier::First());
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(0);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 0, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 }, {3, 1, 1}, {2, 1, 1}, { 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

	state.GetBoard().Get(state::PlayerIdentifier::First()).GetResource().Refill();
	AddHandCard(Cards::ID_EX1_019, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 4;
	parameter_getter.next_specified_target_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(0);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 5 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {4, 2, 2}, { 3, 2, 2 },{ 3, 1, 1 },{ 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 1);

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);
		parameter_getter.next_defender_count = 2;
		parameter_getter.next_defender_idx = 0;
		assert(!engine::FlowControl::ValidActionGetter(state2).IsAttackable(
			state2.GetBoard().Get(state::PlayerIdentifier::First()).minions_.Get(3)
		));
	}

	random.next_rand = 0;
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 2, 2 },{ 3, 2, 2 },{ 3, 1, 1 },{ 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	AddHandCard(Cards::ID_CS2_124, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 3, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 2, 2 },{ 3, 2, 2 },{ 3, 1, 1 },{ 3, 2, 2 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 }, {4, 1, 1} });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 5;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().Get(state::PlayerIdentifier::Second()).minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 3, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 }});
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().Refill();
	AddHandCard(Cards::ID_DS1_055, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 }, {5, 5, 5} });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().Refill();
	AddHandCard(Cards::ID_CS2_226, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 }, {7, 6, 6}, { 5, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().Get(state::PlayerIdentifier::Second()).minions_.Get(0));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 6, 6, 6 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().Refill();
	AddHandCard(Cards::ID_EX1_399, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 6, 6, 6 },{2, 7, 7}, { 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().Refill();
	AddHandCard(Cards::ID_CS2_189, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_specified_target_count = 8;
	parameter_getter.next_specified_target_idx = 6;
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { {1, 1, 1},  { 6, 6, 6 },{ 5, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	AddHandCard(Cards::ID_CS2_189, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_specified_target_count = 9;
	parameter_getter.next_specified_target_idx = 7;
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 }, {1, 1, 1},{ 6, 6, 6 },{ 8, 5, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().Refill();
	AddHandCard(Cards::ID_EX1_593, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 5 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 }, {4, 4, 4}, { 1, 1, 1 },{ 6, 6, 6 },{ 8, 5, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 6, 6 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 6 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 4, 4, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 5, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 2);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 6, 6 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 4, 4, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 5, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	AddHandCard(Cards::ID_CS2_222, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 6, 6 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 }, {6, 6, 6}, { 5, 5, 5 },{ 2, 2, 2 },{ 7, 7, 7 },{ 9, 6, 8 },{ 5, 6, 6 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 1);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 },{ 2, 1, 1 },{ 2, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 },{ 6, 6, 6 },{ 5, 5, 5 },{ 2, 2, 2 },{ 7, 7, 7 },{ 9, 6, 8 },{ 5, 6, 6 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 3;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 3;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 3, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 2, 2 },{ 6, 6, 6 }, {5, 1, 5}, { 2, 2, 2 },{ 7, 7, 7 },{ 9, 6, 8 },{ 5, 6, 6 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	AddHandCard(Cards::ID_CS2_203, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 8;
	parameter_getter.next_specified_target_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 4, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {2, 1, 1}, { 3, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 6, 6, 6 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	AddHandCard(Cards::ID_CS2_188, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 9;
	parameter_getter.next_specified_target_idx = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 3, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {1, 1, 1}, { 2, 1, 1 },{ 5, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 6, 6, 6 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	AddHandCard(Cards::ID_CS2_188, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 10;
	parameter_getter.next_specified_target_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {1, 1, 1}, { 1, 1, 1 },{ 2, 1, 1 },{ 7, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 6, 6, 6 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(3));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	AddHandCard(Cards::ID_CS2_188, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 9;
	parameter_getter.next_specified_target_idx = 4;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(2);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 1, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 7 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {1, 1, 1}, { 1, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 6, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 3);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 1, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 8, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 1, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_390, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 1, 7 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 1, 1, 1 },{ 1, 1, 1 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { {2, 3, 3}, { 1, 1, 1 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 2);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(0));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 8, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 }, { 1, 1, 1 }, { 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 5, 2, 3 },{ 1, 1, 1 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(0));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 8, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 5, 1, 3 },{ 1, 1, 1 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(0));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 8, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_008, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 30, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 8 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 29, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 4, 1, 4 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 1;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetSecond().minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 29, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 1, 1 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 1;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetSecond().minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 29, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 8 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), {{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	// if a 1/1 is buffed to 2/2 by Stormwind Champion then damaged for 1,
	// if it is silenced, it does not temporarily become a 1/1,
	// get buffed by Stormwind Champion again and thus become an undamaged 2/2.
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 25, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 29, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_CS2_222, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 25, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 29, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {6, 6, 6},  { 3, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 1;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 25, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 29, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 9 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 6, 6, 6 },{ 3, 1, 2} });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 25, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 27, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_CS2_203, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_specified_target_count = 5;
	parameter_getter.next_specified_target_idx = 1;
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 25, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 27, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 9 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 }, { 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 23, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 27, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_350, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 23, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 27, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 3, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {8, 8, 8}, { 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_350, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 23, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 27, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 3, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {8, 8, 8}, { 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_specified_target_count = 10;
	parameter_getter.next_specified_target_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpHeroPower);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 27, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 1, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);
		assert(!engine::FlowControl::ValidActionGetter(state2).HeroPowerUsable());
	}

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::First());
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 27, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 4);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	random.called_times = 0;
	random.next_rand = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 12);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 15, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_specified_target_count = 10;
	parameter_getter.next_specified_target_idx = 5;
	engine::FlowControl::Manipulate(state, flow_context).HeroPower(state::PlayerIdentifier::First()).SetUsable();
	parameter_getter.SetMainOp(engine::kMainOpHeroPower);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_minion_put_location = 0;
	AddHandCard(Cards::ID_CFM_807, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 4, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {4, 5, 5}, { 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);
		parameter_getter.next_specified_target_count = 11;
		parameter_getter.next_specified_target_idx = 0;
		assert(!engine::FlowControl::ValidActionGetter(state2).HeroPowerUsable());
	}

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::First());
	random.called_times = 0;
	random.next_rand = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 12);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 11, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 3, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 5, 5 },{ 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_specified_target_count = 11;
	parameter_getter.next_specified_target_idx = 6;
	engine::FlowControl::Manipulate(state, flow_context).HeroPower(state::PlayerIdentifier::First()).SetUsable();
	parameter_getter.SetMainOp(engine::kMainOpHeroPower);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 19, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 1, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 5, 5 },{ 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 6, 6, 6 },{ 8, 6, 7 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetFirst().GetResource().Refill();
	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);
		parameter_getter.next_specified_target_count = 11;
		parameter_getter.next_specified_target_idx = 0;
		assert(!engine::FlowControl::ValidActionGetter(state2).HeroPowerUsable());
	}

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::First());
	random.called_times = 0;
	random.next_rand = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 12);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 19, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 9, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 5, 5 },{ 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 }, { 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_specified_target_count = 9;
	parameter_getter.next_specified_target_idx = 6;
	parameter_getter.SetMainOp(engine::kMainOpHeroPower);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 27, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 5, 5 },{ 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 24, 0, 0); // next fatigue: 4
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 5, 5 },{ 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_OG_121, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 24, 0, 0); // next fatigue: 4
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 3, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 5, 5 },{ 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 },{7, 7, 7} });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::Second());
	random.called_times = 0;
	random.next_rand = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 3);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0); // next fatigue: 4
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 3, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 4, 2, 5 },{ 8, 8, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::Second());
	random.called_times = 0;
	random.next_rand = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 3);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0); // next fatigue: 4
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 8, 7, 8 },{ 8, 8, 8 },{ 6, 6, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::Second());
	random.called_times = 0;
	random.next_rand = 3;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 3);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0); // next fatigue: 4
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 1, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 8, 7, 8 },{ 8, 8, 8 },{ 6, 3, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::Second());
	random.called_times = 0;
	random.next_rand = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 3);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0); // next fatigue: 4
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 8, 4, 8 },{ 8, 8, 8 },{ 6, 3, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetSecond().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::Second());
	random.called_times = 0;
	random.next_rand = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 3);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0); // next fatigue: 4
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 8, 1, 8 },{ 8, 8, 8 },{ 6, 3, 6 },{ 3, 1, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::Second());
	random.called_times = 0;
	random.next_rand = 3;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 3);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0); // next fatigue: 3
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0); // next fatigue: 4
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 8, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	
	AddHandCard(Cards::ID_EX1_250, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 30, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 23, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 3, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { {7, 8, 8}, { 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetNextOverload() == 3);

	state.GetBoard().GetFirst().SetFatigueDamage(0);
	state.GetBoard().GetSecond().SetFatigueDamage(0);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 22, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 7, 8, 8 },{ 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetCurrentOverloaded() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetNextOverload() == 0);

	AddHandCard(Cards::ID_EX1_250, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 22, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { {7, 8, 8}, { 7, 8, 8 },{ 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetCurrentOverloaded() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetNextOverload() == 3);

	AddHandCard(Cards::ID_OG_026, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 29, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 22, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 3, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { {3, 2, 2}, { 7, 8, 8 },{ 7, 8, 8 },{ 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetCurrentOverloaded() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetNextOverload() == 0);

	state.GetBoard().GetFirst().SetFatigueDamage(0);
	state.GetBoard().GetSecond().SetFatigueDamage(0);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 28, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 8, 8 },{ 7, 8, 8 },{ 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetCurrentOverloaded() == 0);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).GetResource().GetNextOverload() == 0);

	state.GetBoard().GetFirst().SetFatigueDamage(0);
	state.GetBoard().GetSecond().SetFatigueDamage(0);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 8, 8 },{ 7, 8, 8 },{ 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_CS2_142, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 8, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {2, 2, 2}, { 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 8, 8 },{ 7, 8, 8 },{ 2, 1, 1 },{ 4, 5, 5 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::First());
	random.called_times = 0;
	random.next_rand = 3;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 16);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 },{ 7, 1, 7 },{ 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 }, {7, 8, 8}, { 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);
		parameter_getter.next_defender_count = 1;
		parameter_getter.next_defender_idx = 0;
		parameter_getter.SetMainOp(engine::kMainOpAttack);
		parameter_getter.AnalyzeTargets(state2);
		parameter_getter.SetAttacker(state2.GetBoard().GetFirst().minions_.Get(1));
		controller2.PerformAction();
	}

	parameter_getter.next_defender_count = 1;
	parameter_getter.next_defender_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 }, { 7, 7, 7 },{ 2, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 1, 8 },{ 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);
		parameter_getter.next_defender_count = 1;
		parameter_getter.next_defender_idx = 0;
		parameter_getter.SetMainOp(engine::kMainOpAttack);
		parameter_getter.AnalyzeTargets(state2);
		parameter_getter.SetAttacker(state2.GetBoard().GetFirst().minions_.Get(2));
		controller2.PerformAction();
	}

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(2));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 21, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 },{ 7, 7, 7 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetFirst().minions_.Get(1));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 14, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 },{ 7, 7, 7 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	parameter_getter.next_specified_target_count = 6;
	parameter_getter.next_specified_target_idx = 3;
	parameter_getter.SetMainOp(engine::kMainOpHeroPower);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 },{ 7, 7, 7 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_246, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_specified_target_count = 4;
	parameter_getter.next_specified_target_idx = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 2, 2 },{0, 1, 1} });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_CS2_222, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 3, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {6, 6, 6}, { 3, 3, 3 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_246, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_specified_target_count = 5;
	parameter_getter.next_specified_target_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 0, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 6, 6, 6 },{ 3, 3, 3 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_246, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_specified_target_count = 5;
	parameter_getter.next_specified_target_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 0, 1, 1 },{ 2, 2, 2 },{ 0, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 7, 5, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_246, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_specified_target_count = 5;
	parameter_getter.next_specified_target_idx = 4;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 4, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 0, 1, 1 },{ 2, 2, 2 },{ 0, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 0, 1, 1 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_564, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_specified_target_count = 5;
	parameter_getter.next_specified_target_idx = 3;
	parameter_getter.next_minion_put_location = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 0, 1, 1 },{ 2, 2, 2 }, {3, 2, 2}, { 0, 1, 1 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 0, 1, 1 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);

		parameter_getter.next_defender_count = 1;
		parameter_getter.next_defender_idx = 0;
		assert(!engine::FlowControl::ValidActionGetter(state2).IsAttackable(
			state2.GetBoard().GetFirst().minions_.Get(2)
		));
	}

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);

		parameter_getter.next_defender_count = 1;
		parameter_getter.next_defender_idx = 0;
		assert(!engine::FlowControl::ValidActionGetter(state2).IsAttackable(
			state2.GetBoard().GetFirst().minions_.Get(1)
		));
	}

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_CS2_222, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 3, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {6, 6, 6}, { 1, 2, 2 },{ 3, 3, 3 },{ 4, 3, 3 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 0, 1, 1 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_564, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_specified_target_count = 7;
	parameter_getter.next_specified_target_idx = 0;
	parameter_getter.next_minion_put_location = 4;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 18, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 7, 7 },{ 2, 3, 3 },{ 4, 4, 4 },{ 5, 4, 4 },{7, 7, 7}, { 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 0, 1, 1 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetFirst().SetFatigueDamage(0);
	state.GetBoard().GetSecond().SetFatigueDamage(0);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 7, 7 },{ 2, 3, 3 },{ 4, 4, 4 },{ 5, 4, 4 },{ 7, 7, 7 },{ 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 2, 2 },{ 0, 1, 1 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);

		parameter_getter.next_defender_count = 2;
		parameter_getter.next_defender_idx = 0;
		parameter_getter.SetMainOp(engine::kMainOpAttack);
		parameter_getter.AnalyzeTargets(state2);
		parameter_getter.SetAttacker(state2.GetBoard().GetSecond().minions_.Get(0));
		controller2.PerformAction();
	}

	parameter_getter.next_defender_count = -1;
	parameter_getter.next_defender_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpAttack);
	parameter_getter.AnalyzeTargets(state);
	parameter_getter.SetAttacker(state.GetBoard().GetSecond().minions_.Get(0));
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 7, 7 },{ 4, 4, 4 },{ 5, 4, 4 },{ 7, 7, 7 },{ 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 0, 1, 1 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_277, flow_context, state, state::PlayerIdentifier::Second());
	random.called_times = 0;
	random.next_rand = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	assert(random.called_times == 3);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 9, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 7, 7 },{ 4, 1, 4 },{ 5, 4, 4 },{ 7, 7, 7 },{ 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 0, 1, 1 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_564, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_specified_target_count = 6;
	parameter_getter.next_specified_target_idx = 0;
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 7, 7 },{ 4, 1, 4 },{ 5, 4, 4 },{ 7, 7, 7 },{ 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 2, 2 }, {6, 6, 6} });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetSecond().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_564, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_specified_target_count = 7;
	parameter_getter.next_specified_target_idx = 1;
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 7, 7 },{ 4, 1, 4 },{ 5, 4, 4 },{ 7, 7, 7 },{ 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 1, 2, 2 },{ 6, 6, 6 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetSecond().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_564, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_specified_target_count = 7;
	parameter_getter.next_specified_target_idx = 0;
	parameter_getter.next_minion_put_location = 1;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 5, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7, 7, 7 },{ 4, 1, 4 },{ 5, 4, 4 },{ 7, 7, 7 },{ 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 3, 3 },{7, 7, 7}, { 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_EX1_246, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_specified_target_count = 8;
	parameter_getter.next_specified_target_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 0, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 2, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 2, 2 },{ 3, 1, 3 },{ 4, 3, 3 },{ 6, 6, 6 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 3, 3 },{ 7, 7, 7 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	engine::FlowControl::Manipulate(state, flow_context)
		.Player(state::PlayerIdentifier::Second())
		.ReplaceHeroPower(Cards::ID_CS2_102); // warrior hero power

	parameter_getter.SetMainOp(engine::kMainOpHeroPower);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 0, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 2, 2 },{ 3, 1, 3 },{ 4, 3, 3 },{ 6, 6, 6 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 3, 3 },{ 7, 7, 7 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	state.GetBoard().GetSecond().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_294, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 7, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 2, 2 },{ 3, 1, 3 },{ 4, 3, 3 },{ 6, 6, 6 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 2, 3, 3 },{ 7, 7, 7 },{ 7, 7, 7 } });
	assert(state.GetBoard().GetSecond().secrets_.Exists(Cards::ID_EX1_294));
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	{
		auto state2 = state;
		auto flow_context2 = flow_context;
		engine::FlowControl::FlowController controller2(state2, flow_context2);
		state2.GetBoard().GetSecond().GetResource().Refill();
		AddHandCard(Cards::ID_EX1_294, flow_context2, state2, state::PlayerIdentifier::Second());
		assert(!engine::FlowControl::ValidActionGetter(state2).IsPlayable(4));
	}

	AddHandCard(Cards::ID_NEW1_025, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 6, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 2, 2 },{ 3, 1, 3 },{ 4, 3, 3 },{ 6, 6, 6 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { {3, 4, 4}, { 2, 3, 3 },{ 7, 7, 7 },{ 7, 7, 7 } });
	assert(state.GetBoard().GetSecond().secrets_.Exists(Cards::ID_EX1_294));
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);

	AddHandCard(Cards::ID_CS2_106, flow_context, state, state::PlayerIdentifier::Second());
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(4);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 27, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 2, 2 },{ 3, 1, 3 },{ 4, 3, 3 },{ 6, 6, 6 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 4, 4 },{ 2, 3, 3 },{ 7, 7, 7 },{ 7, 7, 7 } });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetBoard().GetSecond().secrets_.Exists(Cards::ID_EX1_294));
	assert(state.GetBoard().GetSecond().GetWeaponRef().IsValid());
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 2);

	state.GetBoard().GetFirst().SetFatigueDamage(0);
	state.GetBoard().GetSecond().SetFatigueDamage(0);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 10, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 1, 2, 2 },{ 3, 1, 3 },{ 4, 3, 3 },{ 6, 6, 6 },{ 1, 2, 2 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 3, 4, 4 },{ 2, 3, 3 },{ 7, 7, 7 },{ 7, 7, 7 } });
	assert(state.GetBoard().GetSecond().secrets_.Exists(Cards::ID_EX1_294));
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 2);

	AddHandCard(Cards::ID_CS2_222, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 3;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 3, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 3, 3 },{ 4, 2, 4 },{ 5, 4, 4 },{7, 7, 7}, { 7, 7, 7 },{ 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 4, 5, 5 },{ 3, 4, 4 },{ 8, 8, 8 },{ 8, 8, 8 }, {8, 8, 8} });
	assert(state.GetBoard().GetSecond().secrets_.Exists(Cards::ID_EX1_294) == false);
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 2);

	AddHandCard(Cards::ID_NEW1_025, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 3;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 2, 3, 3 },{ 4, 2, 4 },{ 5, 4, 4 },{3, 4, 4}, { 7, 7, 7 },{ 7, 7, 7 },{ 2, 3, 3 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), { { 4, 5, 5 },{ 3, 4, 4 },{ 8, 8, 8 },{ 8, 8, 8 },{ 8, 8, 8 } });
	assert(state.GetBoard().GetSecond().secrets_.Exists(Cards::ID_EX1_294) == false);
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 1);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_312, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { });
	CheckMinions(state, state::PlayerIdentifier::Second(), { });
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 1);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_NEW1_038, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {7,7,7} });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 1);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_NEW1_038, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 2, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7,7,7 }, {7,7,7} });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 1);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_CS2_203, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 2;
	parameter_getter.next_specified_target_idx = 0;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 7, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {2, 1, 1}, { 7,7,7 },{ 7,7,7 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 1);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_564, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 3;
	parameter_getter.next_specified_target_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {7,7,7}, { 2, 1, 1 },{ 7,7,7 },{ 7,7,7 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 1);

	state.GetBoard().GetFirst().GetResource().Refill();
	AddHandCard(Cards::ID_EX1_564, flow_context, state, state::PlayerIdentifier::First());
	parameter_getter.next_minion_put_location = 0;
	parameter_getter.next_specified_target_count = 4;
	parameter_getter.next_specified_target_idx = 2;
	parameter_getter.SetMainOp(engine::kMainOpPlayCard);
	parameter_getter.SetHandCard(3);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 2, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 4, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { {7,7,7}, { 7,7,7 },{ 2, 1, 1 },{ 7,7,7 },{ 7,7,7 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 1);

	state.GetBoard().GetFirst().SetFatigueDamage(0);
	state.GetBoard().GetSecond().SetFatigueDamage(0);
	parameter_getter.SetMainOp(engine::kMainOpEndTurn);
	if (controller.PerformAction() != engine::kResultNotDetermined) assert(false);
	CheckHero(state, state::PlayerIdentifier::First(), 26, 0, 0);
	CheckHero(state, state::PlayerIdentifier::Second(), 17, 1, 0);
	CheckCrystals(state, state::PlayerIdentifier::First(), { 5, 10 });
	CheckCrystals(state, state::PlayerIdentifier::Second(), { 10, 10 });
	CheckMinions(state, state::PlayerIdentifier::First(), { { 7,7,7 },{ 8,8,8 },{ 2, 1, 1 },{ 7,7,7 },{ 8,8,8 } });
	CheckMinions(state, state::PlayerIdentifier::Second(), {});
	assert(state.GetBoard().Get(state::PlayerIdentifier::First()).hand_.Size() == 3);
	assert(state.GetBoard().Get(state::PlayerIdentifier::Second()).hand_.Size() == 4);
	assert(state.GetCard(state.GetBoard().GetSecond().GetWeaponRef()).GetHP() == 1);
}