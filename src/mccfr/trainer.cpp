#include "../../include/mccfr/trainer.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>

// Removed depth limit - let CFR explore freely
// static const int MAX_CFR_DEPTH = 50;

Trainer::Trainer(GameState *g) : game(g), em() {}

Trainer::~Trainer() {
  for (auto &itr : node_map) {
    delete itr.second;
  }
}

// Helper function to deal random hole cards
void Trainer::deal_random_hole_cards(GameState &state, std::mt19937 &gen) {
  // Create a full deck
  std::vector<Card> deck;
  for (int r = 0; r < 13; ++r) {
    for (int s = 0; s < 4; ++s) {
      deck.emplace_back(static_cast<Rank>(r), static_cast<Suit>(s));
    }
  }

  // Shuffle the deck
  std::shuffle(deck.begin(), deck.end(), gen);

  // Deal 2 cards to each player
  int card_idx = 0;
  for (auto &player : state.players) {
    player.hole_cards.clear();
    player.hole_cards.push_back(deck[card_idx++]);
    player.hole_cards.push_back(deck[card_idx++]);
  }
}

// Helper function to deal random community cards
void Trainer::deal_random_community_cards(GameState &state, int num_cards,
                                          std::mt19937 &gen) {
  // Create a full deck
  std::vector<Card> deck;
  for (int r = 0; r < 13; ++r) {
    for (int s = 0; s < 4; ++s) {
      deck.emplace_back(static_cast<Rank>(r), static_cast<Suit>(s));
    }
  }

  // Remove already dealt cards (player hole cards + existing community cards)
  auto is_dealt = [&](const Card &c) {
    // Check player cards
    for (const auto &p : state.players) {
      for (const auto &pc : p.hole_cards) {
        if (pc.rank == c.rank && pc.suit == c.suit)
          return true;
      }
    }
    // Check existing community cards
    for (const auto &cc : state.community_cards) {
      if (cc.rank == c.rank && cc.suit == c.suit)
        return true;
    }
    return false;
  };

  deck.erase(std::remove_if(deck.begin(), deck.end(), is_dealt), deck.end());

  // Shuffle remaining deck
  std::shuffle(deck.begin(), deck.end(), gen);

  // Deal the specified number of cards
  for (int i = 0; i < num_cards && i < (int)deck.size(); ++i) {
    state.community_cards.push_back(deck[i]);
  }
}

void Trainer::train(int iterations, int num_players) {
  if (!game) {
    return;
  }

  std::random_device rd;
  std::mt19937 gen(rd());

  // Configuration options for sampling
  std::vector<int> player_counts = {2, 3, 4, 5, 6};
  std::vector<double> stack_bb_options = {10, 25, 50, 100, 200};

  for (int i = 0; i < iterations; ++i) {

    if (i % 500 == 0 && i > 0) {
      std::cout << "Iteration " << i << "/" << iterations << " (" << std::fixed
                << std::setprecision(1) << (100.0 * i / iterations) << "%) - "
                << node_map.size() << " nodes\n";
    }

    // Randomly sample configuration
    int sampled_players = player_counts[gen() % player_counts.size()];
    double stack_bb = stack_bb_options[gen() % stack_bb_options.size()];

    // Fixed blinds (abstraction normalizes anyway)
    double bb = 2.0;
    double sb = 1.0;
    double stack = stack_bb * bb;

    // External sampling: traverse from each player's perspective
    for (int p = 0; p < sampled_players; ++p) {
      // Create fresh initial state for each traversal
      GameState fresh_state(nullptr, game->equity_module);
      fresh_state.num_players = sampled_players;
      fresh_state.small_blind_amount = sb;
      fresh_state.big_blind_amount = bb;
      fresh_state.dealer_index = 0;

      // Initialize players
      fresh_state.players.clear();
      for (int pid = 0; pid < sampled_players; ++pid) {
        fresh_state.players.emplace_back(pid, stack, false);
      }

      fresh_state.start_hand();

      // Deal random hole cards for all players
      deal_random_hole_cards(fresh_state, gen);

      // Traverse the game tree from this player's perspective
      std::vector<double> pi(sampled_players, 1.0);
      cfr(fresh_state, p, pi, 1.0, gen);
    }
  }
  std::cout << "Training complete: " << iterations << " iterations\n";
}

std::vector<double> Trainer::calculate_payoffs(GameState &state) {
  int pot = state.pot_size;
  std::vector<int> ranks(state.num_players, 0);
  int max = 0;
  for (int i = 0; i < state.num_players; ++i) {
    Player* p = state.get_player(i);
    if (p->is_folded) {
        ranks[i] = -state.get_player(i)->current_bet;
    }

    std::vector<Card> hand;

    hand.insert(hand.end(), p->hole_cards.begin(), p->hole_cards.end());
    hand.insert(hand.end(), state.community_cards.begin(), state.community_cards.end());

    ranks[i] = em.evaluate_7_cards(hand);
    max = std::max(max, ranks[i]);
  }

  std::vector<double> terminal_payoffs(state.num_players, 0);

  for (int i = 0; i < state.num_players; ++i) {
    if (ranks[i] == max)
      terminal_payoffs[i] = pot;
    else
      terminal_payoffs[i] = -state.get_player(i)->current_bet;
  }

  return terminal_payoffs;
}

double Trainer::get_terminal_payoff(GameState &state, int player_id) {
  std::vector<double> payoffs = calculate_payoffs(state);
  return payoffs[player_id];
}


double Trainer::cfr(GameState &state, int player_id, std::vector<double> &pi, double chance_prob,
                    std::mt19937 &gen, int depth) {

  // Terminal state: return utility
  if (state.is_terminal()) {
    Player *p = state.get_player(player_id);
    if (!p)
      return 0.0;
    return get_terminal_payoff(state, player_id);
  }

  // // Handle betting round transitions
  // if (state.is_betting_round_over() && state.stage != Stage::SHOWDOWN) {
  //   // Check if we need to deal cards (chance node)
  //   if (state.stage == Stage::PREFLOP && state.community_cards.empty()) {
  //     // Deal flop (3 cards)
  //     deal_random_community_cards(state, 3, gen);
  //     state.stage = Stage::FLOP;
  //     state.next_street();
  //   } else if (state.stage == Stage::FLOP &&
  //              state.community_cards.size() == 3) {
  //     // Deal turn (1 card)
  //     deal_random_community_cards(state, 1, gen);
  //     state.stage = Stage::TURN;
  //     state.next_street();
  //   } else if (state.stage == Stage::TURN &&
  //              state.community_cards.size() == 4) {
  //     // Deal river (1 card)
  //     deal_random_community_cards(state, 1, gen);
  //     state.stage = Stage::RIVER;
  //     state.next_street();
  //   } else {
  //     state.next_street();
  //   }
  // }

  // Check terminal again after street transition
  if (state.is_terminal()) {
    Player *p = state.get_player(player_id);
    if (!p)
      return 0.0;
    return get_terminal_payoff(state, player_id);
  }

  Player *curr_player = state.get_current_player();
  if (!curr_player) {
    return 0.0;
  }

  if (state.is_chance()) {
    double value_sum = 0.0;
    auto chance_outcomes = state.get_chance_outcomes(state); 
    for (auto &out : chance_outcomes) {
      GameState next_state = out.first;
      double p = out.second; 
      value_sum += p * cfr(next_state, player_id, pi, chance_prob * p, gen, depth + 1);
    }
    return value_sum;
  }

  std::string info_set = state.compute_information_set(curr_player->id);
  std::vector<Action> legal_actions = state.get_legal_actions();

  if (legal_actions.empty())
    return 0.0;

  if (node_map.find(info_set) == node_map.end()) {
    node_map[info_set] = new Node(legal_actions.size());
  }
  Node *node = node_map[info_set];

  // External Sampling: Update strategy sum only for the traverser (player_id)
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

    double opponents_reach = 1.0;
    for (auto& p : state.players) {
      if (p.id == player_id) continue;
      opponents_reach *= pi[p.id];
    }
    double counterfactual_weight_prefactor = opponents_reach * chance_prob;

    for (size_t i = 0; i < legal_actions.size(); ++i) {
      GameState next_state = state;
      next_state.apply_action(legal_actions[i]);

      double old_pi_curr = pi[player_id];
      pi[player_id] = old_pi_curr * strategy[i];

      utils[i] = cfr(next_state, player_id, pi, chance_prob, gen, depth + 1);
      pi[player_id] = old_pi_curr;

      node_util += strategy[i] * utils[i];
    }

    // Update Regrets
    for (size_t i = 0; i < legal_actions.size(); ++i) {
      double regret = utils[i] - node_util;
      node->update_regret_sum(i, counterfactual_weight_prefactor * regret);
    }
    return node_util;
  } else {
    // Opponent: Sample ONE action
    std::discrete_distribution<> dist(strategy.begin(), strategy.end());
    int sampled = dist(gen);

    double old_pi = pi[curr_player->id];
    pi[curr_player->id] = old_pi * strategy[sampled];

    GameState next_state = state;
    next_state.apply_action(legal_actions[sampled]);

    double res = cfr(next_state, player_id, pi, chance_prob, gen, depth + 1);
    pi[player_id] = old_pi;
    return res;
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

    // CRITICAL FIX: Save RAW strategy_sum, not normalized average
    std::vector<double> strat_sum = node->get_strategy_sum();
    size_t num_actions = strat_sum.size();
    out.write(reinterpret_cast<const char *>(&num_actions),
              sizeof(num_actions));
    out.write(reinterpret_cast<const char *>(strat_sum.data()),
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

    std::vector<double> strat_sum(num_actions);
    in.read(reinterpret_cast<char *>(strat_sum.data()),
            num_actions * sizeof(double));

    // Create node and restore the RAW strategy_sum
    Node *node = new Node(num_actions);
    node->set_strategy_sum(strat_sum);
    node_map[key] = node;
  }

  in.close();
  std::cout << "Loaded " << map_size << " nodes from " << filename << "\n";
}
