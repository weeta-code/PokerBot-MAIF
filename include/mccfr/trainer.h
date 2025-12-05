#ifndef TRAINER_H
#define TRAINER_H

#include "game_state.h"
#include "node.h"
#include "equity.h"
#include <random>
#include <unordered_map>
#include <vector>

using InfoSetKey = std::string;

class Trainer {
private:
  GameState *game;
  EquityModule em;
  std::unordered_map<InfoSetKey, Node *> node_map;

  double cfr(GameState &state, int player_id, std::vector<double> &pi, double history_prob,
             std::mt19937 &gen, int depth = 0);

  std::vector<double> calculate_payoffs(GameState &state);

  double get_terminal_payoff(GameState &state, int player_id);

  // Helper functions for card dealing
  void deal_random_hole_cards(GameState &state, std::mt19937 &gen);
  void deal_random_community_cards(GameState &state, int num_cards,
                                   std::mt19937 &gen);

public:
  explicit Trainer(GameState *game);

  ~Trainer();

  void train(int iterations, int num_players = 2);

  std::vector<double> get_strategy(const std::string &info_set);

  Action get_action_recommendation(GameState &state, int player_id,
                                   std::vector<double> &probabilities);

  void save_to_file(const std::string &filename);
  void load_from_file(const std::string &filename);
};

#endif