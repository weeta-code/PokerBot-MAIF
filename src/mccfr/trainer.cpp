#include "../../include/mccfr/trainer.h"
#include <iostream>
#include <random>
// #include <fstream>
#include <ctime>
// #include <set>
// #include <iomanip>

Trainer::Trainer(GameState *g) : game(g) {}

Trainer::~Trainer() {
  for (auto &itr : node_map) {
    delete itr.second;
  }
}

void Trainer::train(int iterations) {
  std::cerr << "TRAIN: Entered. game ptr = " << game << "\n";
  std::cerr.flush();

  if (!game) {
    std::cerr << "ERROR: game pointer is NULL!\n";
    return;
  }

  std::cerr << "TRAIN: Checking num_players...\n";
  std::cerr.flush();

  if (game->num_players == 0) {
    std::cerr << "TRAIN: Initializing players...\n";
    game->num_players = 2;
    game->small_blind_amount = 10;
    game->big_blind_amount = 20;
    game->dealer_index = 0;
    game->players.clear();
    for (int i = 0; i < game->num_players; ++i) {
      game->players.emplace_back(i, 1000, false);
    }
    std::cerr << "TRAIN: Players initialized.\n";
  }

  std::cout << "Starting training loop with " << game->num_players << " players\n";
  std::cout << "Equity module ptr: " << (game->equity_module ? "valid" : "NULL") << "\n";
  std::cout << "Risk profiler ptr: " << (game->risk_profiler ? "valid" : "NULL") << "\n";

  for (int i = 0; i < iterations; ++i) {
    if (i == 0) {
      std::cout << "Starting iteration 0...\n";
    }

    if (i % 500 == 0 && i > 0) {
      std::cout << "Iteration " << i << "/" << iterations << " ("
                << std::fixed << std::setprecision(1) << (100.0 * i / iterations)
                << "%) - " << node_map.size() << " nodes\n";
    }

    game->start_hand();

    if (i == 0) {
      std::cout << "After start_hand: players=" << game->players.size()
                << ", pot=" << game->pot_size << "\n";
      for (int p = 0; p < game->num_players; ++p) {
        std::cout << "  Player " << p << ": cards=" << game->players[p].hole_cards.size()
                  << ", stack=" << game->players[p].stack << "\n";
      }
    }

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
      if (i == 0) {
        std::cout << "About to copy state for player " << p << "\n";
      }

      GameState sim_state = *game;
      sim_state.risk_profiler = nullptr;
      sim_state.equity_module = game->equity_module;

      if (i == 0) {
        std::cout << "State copied. About to call CFR for player " << p << "\n";
        std::cout << "  sim_state: players=" << sim_state.players.size()
                  << ", equity_module=" << (sim_state.equity_module ? "valid" : "NULL") << "\n";
      }

      cfr(sim_state, p, 1.0);

      if (i == 0) {
        std::cout << "CFR returned for player " << p << "\n";
      }
    }

    std::time_t curr_time = std::time(nullptr);

    char* time_file_name = std::ctime(&curr_time);
    
    save_to_file(time_file_name);

  }
  std::cout << "Training complete: " << iterations << " iterations\n";
}

double Trainer::cfr(GameState &state, int player_id, double history_prob) {
  static int terminal_count = 0;
  static int info_set_call_count = 0;
  static int cfr_call_count = 0;
  static std::set<std::string> unique_info_sets;

  cfr_call_count++;

  if (cfr_call_count == 1) {
    std::cout << "FIRST CFR CALL: player_id=" << player_id
              << ", players=" << state.players.size()
              << ", stage=" << (int)state.stage << "\n";
  }

  if (state.is_terminal()) {
    terminal_count++;
    Player *p = state.get_player(player_id);
    if (!p)
      return 0.0;

    double utility = p->stack - 1000.0;

    if (terminal_count <= 50) {
      std::cout << "Terminal #" << terminal_count << ": player " << player_id
                << " stack=" << p->stack << " utility=" << utility
                << " stage=" << (int)state.stage << " pot=" << state.pot_size << "\n";
    }

    return utility;
  }

  if (state.is_betting_round_over()) {
    state.next_street();
    if (state.is_terminal())
      return cfr(state, player_id, history_prob);
  }

  if (cfr_call_count <= 5) {
    std::cout << "CFR call #" << cfr_call_count << ": Getting current player...\n";
    std::cout << "  current_player_index=" << state.current_player_index
              << ", num_players=" << state.num_players << "\n";
  }

  Player *curr_player = state.get_current_player();

  if (!curr_player) {
    std::cout << "ERROR: get_current_player returned nullptr!\n";
    return 0.0;
  }

  if (cfr_call_count <= 5) {
    std::cout << "  Got player " << curr_player->id << ", computing info set...\n";
  }

  std::string info_set = state.compute_information_set(curr_player->id);
  std::vector<Action> legal_actions = state.get_legal_actions();

  info_set_call_count++;
  unique_info_sets.insert(info_set);

  if (info_set_call_count <= 100) {
    std::cout << "InfoSet #" << info_set_call_count << ": \"" << info_set << "\"\n";
    std::cout << "  Stage=" << (int)state.stage << " Pot=" << state.pot_size
              << " Board=" << state.community_cards.size() << " cards\n";
  }

  if (info_set_call_count % 5000 == 0) {
    std::cout << "After " << info_set_call_count << " CFR calls: "
              << unique_info_sets.size() << " unique info sets, "
              << node_map.size() << " nodes in map\n";
  }

  if (legal_actions.empty())
    return 0.0;

  if (node_map.find(info_set) == node_map.end()) {
    node_map[info_set] = new Node(legal_actions.size());
    if (node_map.size() <= 20) {
      std::cout << "Created node #" << node_map.size() << " for info_set: \"" << info_set << "\"\n";
    }
  }
  Node *node = node_map[info_set];

  std::vector<double> strategy =
      node->get_strategy(curr_player->id == player_id ? history_prob : 1.0);

  if (curr_player->id == player_id) {
    std::vector<double> action_utils(legal_actions.size(), 0.0);

    for (size_t i = 0; i < legal_actions.size(); ++i) {
      GameState next_state = state;
      next_state.apply_action(legal_actions[i]);
      action_utils[i] = cfr(next_state, player_id, history_prob * strategy[i]);
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
    next_state.apply_action(legal_actions[sampled]);
    return cfr(next_state, player_id, history_prob * strategy[sampled]);
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

  return legal_actions[selected];
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
