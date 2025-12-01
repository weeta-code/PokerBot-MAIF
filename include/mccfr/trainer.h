#ifndef TRAINER_H
#define TRAINER_H

#include "game_state.h"
#include "node.h"
#include <unordered_map>
#include <vector>

using InfoSetKey = std::string;

class Trainer {
private:
  GameState *game;
  std::unordered_map<InfoSetKey, Node *> node_map;

  double cfr(GameState &state, int player_id, double history_prob);

public:
  explicit Trainer(GameState *game);

  ~Trainer();

  void train(int iterations);

  std::vector<double> get_strategy(const std::string &info_set);

  Action get_action_recommendation(GameState &state, int player_id,
                                   std::vector<double> &probabilities);
};

#endif