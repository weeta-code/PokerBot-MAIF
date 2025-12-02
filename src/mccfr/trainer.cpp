#include "../../include/mccfr/trainer.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>

static const int MAX_CFR_DEPTH = 50;

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

  std::random_device rd;
  std::mt19937 gen(rd());

  game->small_blind_amount = 10;
  game->big_blind_amount = 20;

  for (int i = 0; i < iterations; ++i) {

    if (i % 500 == 0 && i > 0) {
      std::cout << "Iteration " << i << "/" << iterations << " (" << std::fixed
                << std::setprecision(1) << (100.0 * i / iterations) << "%) - "
                << node_map.size() << " nodes\n";
    }

    game->num_players = 2;
    game->dealer_index = 0;
    game->players.clear();
    for (int p = 0; p < game->num_players; ++p) {
      game->players.emplace_back(p, 1000, false);
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

double Trainer::cfr(GameState &state, int player_id, double history_prob,
                    int depth) {
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

  if (cfr_call_count <= 5) {
    std::cout << "CFR call #" << cfr_call_count
              << ": Getting current player...\n";
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
    std::cout << "After " << info_set_call_count
              << " CFR calls: " << unique_info_sets.size()
              << " unique info sets, " << node_map.size() << " nodes in map\n";
  }

  if (legal_actions.empty())
    return 0.0;

  if (node_map.find(info_set) == node_map.end()) {
    node_map[info_set] = new Node(legal_actions.size());
  }
  Node *node = node_map[info_set];

  // External Sampling: Update strategy sum only for the traverser (player_id)
  // The weight is 1.0 because in External Sampling, the traverser's reach prob
  // is 1.0 (we force all actions) and opponent reach prob is accounted for by
  // sampling.
  double weight = (curr_player->id == player_id) ? 1.0 : 0.0;
  std::vector<double> strategy = node->get_strategy(weight);

  if (strategy.size() != legal_actions.size()) {
    std::cerr << "Error: Action size mismatch for info_set: " << info_set
              << "\n";
    std::cerr << "Node has " << strategy.size()
              << " actions, but current state has " << legal_actions.size()
              << "\n";
    return 0.0;
  }

  if (curr_player->id == player_id) {
    // Traverser: Explore ALL actions
    double node_util = 0.0;
    std::vector<double> utils(legal_actions.size());

    for (size_t i = 0; i < legal_actions.size(); ++i) {
      GameState next_state = state;
      next_state.apply_action(legal_actions[i]);

      utils[i] = cfr(next_state, player_id, history_prob, depth + 1);
      node_util += strategy[i] * utils[i];
    }

    // Update Regrets
    for (size_t i = 0; i < legal_actions.size(); ++i) {
      double regret = utils[i] - node_util;
      node->update_regret_sum(i, regret);
    }

    return node_util;
  } else {
    // Opponent: Sample ONE action
    std::random_device rd;
    std::mt19937 gen(rd());
    std::discrete_distribution<> dist(strategy.begin(), strategy.end());
    int sampled = dist(gen);

    GameState next_state = state;
    next_state.apply_action(legal_actions[sampled]);

    return cfr(next_state, player_id, history_prob, depth + 1);
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

  static int recommendation_count = 0;
  recommendation_count++;

  if (recommendation_count <= 5) {
    std::cout << "[DEBUG] Recommendation #" << recommendation_count << "\n";
    std::cout << "  Info set: \"" << info_set << "\"\n";
    std::cout << "  Found in model: "
              << (node_map.find(info_set) != node_map.end() ? "YES" : "NO")
              << "\n";
    std::cout << "  Model size: " << node_map.size() << " nodes\n";
  }

  if (legal_actions.empty()) {
    probabilities.clear();
    return Action(-1, ActionType::FOLD, 0);
  }

  probabilities = get_strategy(info_set);

  if (probabilities.empty()) {
    if (recommendation_count <= 5) {
      std::cout << "  Strategy empty - using uniform\n";
    }
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
    out.write(reinterpret_cast<const char *>(&num_actions),
              sizeof(num_actions));
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
