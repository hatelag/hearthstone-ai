#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <unordered_map>
#include <map>
#include <random>
#include <iostream>
#include <iterator>
#include <queue>
#include <unordered_set>
#include <stdexcept>

#include "mcts.h"

#define EXLPORATION_FACTOR 2.0

static double CalculateSelectionWeight(TreeNode *node, double total_simulations_ln, double exploration_factor)
{
	auto const& node_win = node->wins;
	auto const& node_simulations = node->count;

	double win_rate;
	if (node->equivalent_node == nullptr) {
		// normal node
		win_rate = (double)node_win / node_simulations;
	}
	else {
		// redirect node
		win_rate = (double)node->equivalent_node->wins / node->equivalent_node->count;
	}

	double exploration_term = sqrt(total_simulations_ln / node_simulations);

	return win_rate + exploration_factor * exploration_term;
}

static TreeNode *FindBestChildToExpand(TreeNode *parent_node, double exploration_factor = EXLPORATION_FACTOR)
{
	const TreeNode::children_type &children = parent_node->children;
	const int &total_simulations = parent_node->count;

	TreeNode::children_type::const_iterator it_child = children.begin();

	double total_simulations_ln = log((double)total_simulations);

	TreeNode * max_weight_node = *it_child;

	double max_weight = CalculateSelectionWeight(max_weight_node, total_simulations_ln, exploration_factor);
	++it_child;

	for (; it_child != children.end(); ++it_child) {
		double weight = CalculateSelectionWeight(*it_child, total_simulations_ln, exploration_factor);
		if (weight > max_weight) {
			max_weight = weight;
			max_weight_node = *it_child;
		}
	}

	return max_weight_node;
}

void MCTS::Initialize(unsigned int rand_seed, const GameEngine::Board &board)
{
	srand(rand_seed);

	this->root_node_board = board;
#ifdef DEBUG_SAVE_BOARD
	tree.GetRootNode().board = board;
#endif
	tree.GetRootNode().stage = board.GetStage();
	tree.GetRootNode().stage_type = board.GetStageType();
	tree.GetRootNode().parent = nullptr;
	tree.GetRootNode().wins = 0;
	tree.GetRootNode().count = 0;
	if (tree.GetRootNode().stage_type == GameEngine::STAGE_TYPE_PLAYER) {
		tree.GetRootNode().is_player_node = true;
	} else {
		// Note: if the starting node is a RANDOM node,
		// then the root node's is_player_node doesn't matter
		tree.GetRootNode().is_player_node = false;
	}

	this->board_node_map.Add(board, &tree.GetRootNode());
}

// Note: &board can equal to &new_board for no-copy operation
void MCTS::Select(TreeNode* const& node, GameEngine::Board const& board, TreeNode* & new_node, GameEngine::Board & new_board)
{
	new_node = node;
	new_board = board; // no copy if &board == &new_board

#ifdef DEBUG
	if (new_node->equivalent_node != nullptr) throw std::runtime_error("consistency check failed");
#endif

	this->traversed_nodes.push_back(new_node);

	while (true)
	{
#ifdef DEBUG_SAVE_BOARD
		if (new_board != new_node->board) {
			throw std::runtime_error("board consistency check failed");
		}
#endif

		if (new_node->children.empty()) {
			return;
		}

		if (new_node->stage_type == GameEngine::STAGE_TYPE_GAME_FLOW) {
			return;
		}

		if (!new_node->next_move_getter.Empty()) {
			return;
		}

		new_node = FindBestChildToExpand(new_node);

		this->traversed_nodes.push_back(new_node); // back-propagate should update the node (not the redirected one)
		if (new_node->equivalent_node != nullptr) 
			new_node = new_node->equivalent_node;

		new_board.ApplyMove(new_node->move);
	}
}

void MCTS::GetNextState(TreeNode *node, GameEngine::Move &move, GameEngine::Board &board)
{

#ifdef DEBUG_SAVE_BOARD
	if (node->board != board) {
		throw std::runtime_error("board consistency check failed");
	}
#endif

	if (node->stage_type == GameEngine::STAGE_TYPE_GAME_FLOW)
	{
		move = GameEngine::Move::GetGameFlowMove(this->GetRandom());
		board.ApplyMove(move);
	}
	else {
		if (node->children.empty()) {
			board.GetNextMoves(node->next_move_getter);

#ifdef DEBUG
			if (node->next_move_getter.Empty()) throw std::runtime_error("should at least return one possible move");
#endif
		}

		if (node->next_move_getter.GetNextMove(move) == false) {
			throw std::runtime_error("a node with no non-expanded move should not be selected to be expanded");
		}

		board.ApplyMove(move);
	}
}

// return false if 'new_node' is an expanded node
// Note: &board should not equal to &new_board
bool MCTS::Expand(TreeNode *node, const GameEngine::Board &board, TreeNode* &new_node, GameEngine::Board &new_board)
{
	if (node->stage_type == GameEngine::STAGE_TYPE_GAME_END) {
		// node cannot be expanded further (i.e., game-end)
		new_node = node;
		new_board = board;
		return true;
	}

#ifdef DEBUG
	if (node->equivalent_node != nullptr) throw std::runtime_error("consistency check failed");
#endif

	new_board = board;

	this->GetNextState(node, this->allocated_node->move, new_board);

	TreeNode *found_node = this->board_node_map.Find(new_board, *this);
	if (found_node != nullptr)
	{
		if (found_node->parent == node)
		{
			// expanded before under the same parent
			// --> no need to create a redirect node
		}
		else {
			// expanded before in other paths
			// --> create a redirect node
			new_node = this->allocated_node;
			this->allocated_node = new TreeNode;

			new_node->wins = 0;
			new_node->count = 0;
			new_node->equivalent_node = found_node;
			node->AddChild(new_node);
		}

		new_node = found_node;
		return false;
	}

	new_node = this->allocated_node;
	this->allocated_node = new TreeNode;

	new_node->wins = 0;
	new_node->count = 0;
	new_node->stage = new_board.GetStage();
	new_node->stage_type = new_board.GetStageType();
#ifdef DEBUG_SAVE_BOARD
	new_node->board = new_board;
#endif

	if (new_node->stage_type == GameEngine::STAGE_TYPE_PLAYER) {
		new_node->is_player_node = true;
	}
	else if (new_node->stage_type == GameEngine::STAGE_TYPE_OPPONENT) {
		new_node->is_player_node = false;
	}
	else {
		new_node->is_player_node = node->is_player_node; // follow the parent's status
	}

	node->AddChild(new_node);

	this->board_node_map.Add(new_board, new_node);

	this->traversed_nodes.push_back(new_node);

	return true;
}

bool MCTS::Simulate(GameEngine::Board &board)
{
	while (true) {
		std::vector<GameEngine::Move> next_moves;

		GameEngine::StageType stage_type = board.GetStageType();

		if (stage_type == GameEngine::STAGE_TYPE_GAME_FLOW) {
			board.ApplyMove(GameEngine::Move::GetGameFlowMove(this->GetRandom()));

		} else if (stage_type == GameEngine::STAGE_TYPE_GAME_END) {
			return (board.GetStage() == GameEngine::STAGE_WIN);

		} else {
			GameEngine::Move move;
			board.GetGoodMove(move, this->GetRandom());
			board.ApplyMove(move);
		}
	}
}

void MCTS::BackPropagate(bool is_win)
{
	while (!this->traversed_nodes.empty())
	{
		TreeNode *node = this->traversed_nodes.back();
		this->traversed_nodes.pop_back();

		if (node->is_player_node) {
			if (is_win == true) node->wins++; // from the player's respect
		}
		else {
			if (is_win == false) node->wins++; // from the opponent's respect
		}
		node->count++;
	}
}

void MCTS::Iterate()
{
	TreeNode *node = &this->tree.GetRootNode();
	GameEngine::Board board = this->root_node_board;;

	TreeNode *new_node = nullptr;
	GameEngine::Board new_board;

#ifdef DEBUG
	if (!this->traversed_nodes.empty()) throw std::runtime_error("consistency check failed");
#endif
	
	// loop if expand a duplicated node
	while (true)
	{
		this->Select(node, board, node, board); // directly update to (node, board)

		if (this->Expand(node, board, new_node, new_board)) break;

		node = new_node;
		board = new_board;
	}

	bool is_win = this->Simulate(new_board);
	this->BackPropagate(is_win);
}

MCTS::MCTS()
{
	this->allocated_node = new TreeNode;
}

MCTS::MCTS(const MCTS& rhs)
{
	*this = rhs;
}

MCTS::MCTS(MCTS&& rhs)
{
	*this = std::move(rhs);
}

MCTS & MCTS::operator=(const MCTS& rhs)
{
	std::unordered_map<TreeNode *, TreeNode *> node_update_map;
	auto node_update_callback = [&node_update_map](TreeNode *origin_pointer, TreeNode *new_pointer) {
		node_update_map.insert(std::make_pair(origin_pointer, new_pointer));
	};

	this->root_node_board = rhs.root_node_board;
	this->tree = rhs.tree.Clone(node_update_callback);
	this->board_node_map = rhs.board_node_map;
	this->board_node_map.UpdateNodePointers(node_update_map);

	return *this;
}

MCTS & MCTS::operator=(MCTS&& rhs)
{
	this->root_node_board = std::move(rhs.root_node_board);
	this->tree = std::move(rhs.tree);
	this->board_node_map = std::move(rhs.board_node_map);
	return *this;
}

bool MCTS::operator==(const MCTS& rhs) const
{
	if (this->root_node_board != rhs.root_node_board) return false;
	if (this->tree != rhs.tree) return false;

	// skip checking for internal consistency data
	// if (this->board_node_map != rhs.board_node_map) return false;

	return true;
}

bool MCTS::operator!=(const MCTS& rhs) const
{
	return !(*this == rhs);
}

int MCTS::GetRandom()
{
	return rand();
}