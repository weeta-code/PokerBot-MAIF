#include "../../include/mccfr/trainer.h"
#include <iostream>
#include <random>

Trainer::Trainer(GameState *g) : game(g) {}

Trainer::~Trainer() {
  for (auto &itr : node_map) {
    delete itr.second;
  }
}

void Trainer::train(int iterations) {
  for (int i = 0; i < iterations; ++i) {
    game->start_hand();
    for (int p = 0; p < game->num_players; ++p) {
      GameState sim_state = *game;
      cfr(sim_state, p, 1.0);
    }
  }
}

double Trainer::cfr(GameState &state, int player_id, double history_prob) {
  if (state.is_terminal()) {
    return 0.0;
  }

  if (state.is_betting_round_over()) {
    state.next_street();
    if (state.is_terminal())
      return cfr(state, player_id, history_prob);
  }

  Player *curr_player = state.get_current_player();
  if (!curr_player)
    return 0.0;

  std::string info_set = state.compute_information_set(curr_player->id);
  std::vector<Action> legal_actions = state.get_legal_actions();

  if (legal_actions.empty())
    return 0.0;

  if (node_map.find(info_set) == node_map.end()) {
    node_map[info_set] = new Node(legal_actions.size());
  }
  Node *node = node_map[info_set];

  std::vector<double> strategy =
      node->get_strategy(curr_player->id == player_id ? history_prob : 1.0);

  double util = 0.0;
  std::vector<double> action_utils(legal_actions.size());

  if (curr_player->id == player_id) {
    for (size_t i = 0; i < legal_actions.size(); ++i) {
      GameState next_state = state;
      next_state.apply_action(legal_actions[i]);
      action_utils[i] = cfr(next_state, player_id, history_prob * strategy[i]);
      util += strategy[i] * action_utils[i];
    }

    for (size_t i = 0; i < legal_actions.size(); ++i) {
      double regret = action_utils[i] - util;
      node->update_regret_sum(i, regret);
    }
  } else {
    for (size_t i = 0; i < legal_actions.size(); ++i) {
      GameState next_state = state;
      next_state.apply_action(legal_actions[i]);
      util +=
          strategy[i] * cfr(next_state, player_id, history_prob * strategy[i]);
    }
  }

  return util;
}

std::vector<double> Trainer::get_strategy(const std::string &info_set) {
  if (node_map.find(info_set) != node_map.end()) {
    return node_map[info_set]->get_average_strategy();
  }
  return {};
}

Action Trainer::get_action_recommendation(GameState &state, int player_id,
                                          std::vector<double> &probabilities) {
  std::string info_set = state.compute_information_set(player_id);
  std::vector<Action> legal_actions = state.get_legal_actions();

  if (legal_actions.empty()) {
    probabilities.clear();
    return Action(-1, ActionType::FOLD, 0);
  }

  probabilities = get_strategy(info_set);

  if (probabilities.empty()) {
    probabilities.resize(legal_actions.size(), 1.0 / legal_actions.size());
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::discrete_distribution<> dist(probabilities.begin(), probabilities.end());
  int selected = dist(gen);

  return legal_actions[selected];
}
