#include "../../include/mccfr/trainer.h"
#include <iostream>
#include <random>
#include <fstream>
#include <set>
#include <iomanip>

static const int MAX_CFR_DEPTH = 500;

Trainer::Trainer(GameState *g) : game(g) {}

Trainer::~Trainer() {
  for (auto &itr : node_map) {
    delete itr.second;
  }
}

void Trainer::train(int iterations) {
  if (!game) {
    return;
  }

  if (game->num_players == 0) {
    game->num_players = 2;
    game->small_blind_amount = 10;
    game->big_blind_amount = 20;
    game->dealer_index = 0;
    game->players.clear();
    for (int i = 0; i < game->num_players; ++i) {
      game->players.emplace_back(i, 1000, false);
    }
  }

  for (int i = 0; i < iterations; ++i) {

    if (i % 500 == 0 && i > 0) {
      std::cout << "Iteration " << i << "/" << iterations << " ("
                << std::fixed << std::setprecision(1) << (100.0 * i / iterations)
                << "%) - " << node_map.size() << " nodes\n";
    }

    game->start_hand();


    int sb_pos = (game->dealer_index + 1) % game->num_players;
    int bb_pos = (game->dealer_index + 2) % game->num_players;

    game->players[sb_pos].stack -= game->small_blind_amount;
    game->players[sb_pos].current_bet = game->small_blind_amount;
    game->pot_size += game->small_blind_amount;

    game->players[bb_pos].stack -= game->big_blind_amount;
    game->players[bb_pos].current_bet = game->big_blind_amount;
    game->pot_size += game->big_blind_amount;
    game->current_street_highest_bet = game->big_blind_amount;

    game->current_player_index = (bb_pos + 1) % game->num_players;

    for (int p = 0; p < game->num_players; ++p) {
      GameState sim_state = *game;
      sim_state.risk_profiler = nullptr;
      sim_state.equity_module = game->equity_module;
      cfr(sim_state, p, 1.0);
    }
  }
  std::cout << "Training complete: " << iterations << " iterations\n";
}

double Trainer::cfr(GameState &state, int player_id, double history_prob, int depth) {
  static int terminal_count = 0;
  static int info_set_call_count = 0;
  static int cfr_call_count = 0;
  static std::set<std::string> unique_info_sets;

  cfr_call_count++;

  if (depth > MAX_CFR_DEPTH) {
    return 0.0;
  }


  if (state.is_terminal()) {
    terminal_count++;
    Player *p = state.get_player(player_id);
    if (!p)
      return 0.0;

    double utility = p->stack - 1000.0;


    return utility;
  }

  if (state.is_betting_round_over() && state.stage != Stage::SHOWDOWN) {
    state.next_street();
  }

  if (state.is_terminal()) {
    terminal_count++;
    Player *p = state.get_player(player_id);
    if (!p)
      return 0.0;

    double utility = p->stack - 1000.0;


    return utility;
  }

  if (cfr_call_count % 100 == 0) {
    std::cout << "CFR call #" << cfr_call_count << ": Getting current player...\n";
    std::cout << "  current_player_index=" << state.current_player_index
              << ", num_players=" << state.num_players << "\n";
  }

  Player *curr_player = state.get_current_player();

  if (!curr_player) {
    return 0.0;
  }

  std::string info_set = state.compute_information_set(curr_player->id);
  std::vector<Action> legal_actions = state.get_legal_actions();

  info_set_call_count++;
  unique_info_sets.insert(info_set);

  if (info_set_call_count % 10000 == 0) {
    std::cout << "After " << info_set_call_count << " CFR calls: "
              << unique_info_sets.size() << " unique info sets, "
              << node_map.size() << " nodes in map\n";
  }

  if (legal_actions.empty())
    return 0.0;

  if (node_map.find(info_set) == node_map.end()) {
    node_map[info_set] = new Node(legal_actions.size());
  }
  Node *node = node_map[info_set];

  std::vector<double> strategy =
      node->get_strategy(curr_player->id == player_id ? history_prob : 1.0);

  if (curr_player->id == player_id) {
    std::vector<double> action_utils(legal_actions.size(), 0.0);

    for (size_t i = 0; i < legal_actions.size(); ++i) {
      GameState next_state = state;
      next_state.apply_action(legal_actions[i]);
      action_utils[i] = cfr(next_state, player_id, history_prob * strategy[i], depth + 1);
    }

    double util = 0.0;
    for (size_t i = 0; i < legal_actions.size(); ++i) {
      util += strategy[i] * action_utils[i];
    }

    for (size_t i = 0; i < legal_actions.size(); ++i) {
      double regret = action_utils[i] - util;
      node->update_regret_sum(i, regret);
    }

    return util;
  } else {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::discrete_distribution<> dist(strategy.begin(), strategy.end());
    int sampled = dist(gen);

    GameState next_state = state;

    // solved potential seg fault? making sure idx ix within legal_actions
    int idx = sampled % legal_actions.size();
    next_state.apply_action(legal_actions[idx]);
    return cfr(next_state, player_id, history_prob * strategy[sampled], depth + 1);
  }
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

  // solved potential seg fault? making sure idx ix within legal_actions
  int idx = selected % legal_actions.size();
  return legal_actions[idx];
}

void Trainer::save_to_file(const std::string &filename) {
  std::ofstream out(filename, std::ios::binary);
  if (!out) {
    std::cerr << "Failed to open file for writing: " << filename << "\n";
    return;
  }

  size_t map_size = node_map.size();
  out.write(reinterpret_cast<const char *>(&map_size), sizeof(map_size));

  for (const auto &[key, node] : node_map) {
    size_t key_len = key.size();
    out.write(reinterpret_cast<const char *>(&key_len), sizeof(key_len));
    out.write(key.c_str(), key_len);

    std::vector<double> avg_strat = node->get_average_strategy();
    size_t num_actions = avg_strat.size();
    out.write(reinterpret_cast<const char *>(&num_actions), sizeof(num_actions));
    out.write(reinterpret_cast<const char *>(avg_strat.data()),
              num_actions * sizeof(double));
  }

  out.close();
  std::cout << "Saved " << map_size << " nodes to " << filename << "\n";
}

void Trainer::load_from_file(const std::string &filename) {
  std::ifstream in(filename, std::ios::binary);
  if (!in) {
    std::cerr << "Failed to open file for reading: " << filename << "\n";
    return;
  }

  for (auto &[key, node] : node_map) {
    delete node;
  }
  node_map.clear();

  size_t map_size;
  in.read(reinterpret_cast<char *>(&map_size), sizeof(map_size));

  for (size_t i = 0; i < map_size; ++i) {
    size_t key_len;
    in.read(reinterpret_cast<char *>(&key_len), sizeof(key_len));

    std::string key(key_len, '\0');
    in.read(&key[0], key_len);

    size_t num_actions;
    in.read(reinterpret_cast<char *>(&num_actions), sizeof(num_actions));

    std::vector<double> avg_strat(num_actions);
    in.read(reinterpret_cast<char *>(avg_strat.data()),
            num_actions * sizeof(double));

    Node *node = new Node(num_actions);
    node_map[key] = node;
  }

  in.close();
  std::cout << "Loaded " << map_size << " nodes from " << filename << "\n";
}
