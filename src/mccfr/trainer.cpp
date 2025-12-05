#include "../../include/mccfr/trainer.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>

// Removed depth limit - let CFR explore freely
// static const int MAX_CFR_DEPTH = 50;

Trainer::Trainer(GameState *g) : game(g), em(*(g->equity_module)) {}

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
      std::cout << "Iteration " << i << "/" << iterations
                << " — nodes=" << node_map.size() << "\n";
    }

    // Randomly sample configuration
    int sampled_players = player_counts[gen() % player_counts.size()];
    double stack_bb = stack_bb_options[gen() % stack_bb_options.size()];

    // Fixed blinds (abstraction normalizes anyway)
    double bb = 2.0;
    double sb = 1.0;
    double stack = stack_bb * bb;

    // External sampling: traverse from each player's perspective
    for (int traverser = 0; traverser < sampled_players; ++traverser) {

      GameState s(nullptr, game->equity_module);
      s.num_players = sampled_players;
      s.small_blind_amount = sb;
      s.big_blind_amount = bb;
      s.dealer_index = 0;

      s.players.clear();
      for (int pid = 0; pid < sampled_players; ++pid)
        s.players.emplace_back(pid, stack, false);

      s.start_hand();
      deal_random_hole_cards(s, gen);

      // CFR ENTRY POINT
      cfr(s, traverser,
          1.0, // prob_traverser
          1.0, // prob_opponent
          1.0, // prob_chance
          gen, 0);
    }
  }
  std::cout << "Training complete: " << iterations << " iterations\n";
}

std::vector<double> Trainer::calculate_payoffs(GameState &state) {
  int pot = state.pot_size;
  std::vector<double> payoff(state.num_players, 0.0);

  int best_rank = -1;
  std::vector<int> rank(state.num_players, -9999);

  for (int i = 0; i < state.num_players; ++i) {
    Player *p = state.get_player(i);

    if (p->is_folded) {
      payoff[i] = -p->current_bet;
      continue;
    }

    std::vector<Card> hand;
    hand.insert(hand.end(), p->hole_cards.begin(), p->hole_cards.end());
    hand.insert(hand.end(), state.community_cards.begin(),
                state.community_cards.end());

    rank[i] = em.evaluate_7_cards(hand);
    best_rank = std::max(best_rank, rank[i]);
  }

  for (int i = 0; i < state.num_players; ++i) {
    Player *p = state.get_player(i);

    if (p->is_folded) {
      // already set: payoff[i] = -p->current_bet;
    } else if (rank[i] == best_rank) {
      payoff[i] = pot;
    } else {
      payoff[i] = -p->current_bet;
    }
  }

  return payoff;
}

double Trainer::get_terminal_payoff(GameState &state, int player_id) {
  return calculate_payoffs(state)[player_id];
}

double Trainer::cfr(GameState &state, int traverser, double prob_traverser,
                    double prob_opponent, double prob_chance, std::mt19937 &gen,
                    int depth) {
  //
  // Terminal check
  //
  if (state.is_terminal() || depth > 200)
    return get_terminal_payoff(state, traverser);

  //
  // STREET TRANSITION (your GameState removed its own transitions)
  //
  if (state.is_betting_round_over() && state.stage != Stage::SHOWDOWN) {
    if (state.stage == Stage::PREFLOP && state.community_cards.empty()) {
      deal_random_community_cards(state, 3, gen);
      state.stage = Stage::FLOP;
      state.next_street();
    } else if (state.stage == Stage::FLOP &&
               state.community_cards.size() == 3) {
      deal_random_community_cards(state, 1, gen);
      state.stage = Stage::TURN;
      state.next_street();
    } else if (state.stage == Stage::TURN &&
               state.community_cards.size() == 4) {
      deal_random_community_cards(state, 1, gen);
      state.stage = Stage::RIVER;
      state.next_street();
    } else {
      state.next_street();
    }

    if (state.is_terminal())
      return get_terminal_payoff(state, traverser);
  }

  Player *curr = state.get_current_player();
  int acting = curr->id;

  //
  // Build info set (your GameState no longer appends legal-action count)
  //
  std::string info = state.compute_information_set(acting);

  //
  // Legal actions according to your NEW abstraction system
  //
  auto legal = state.get_legal_actions();
  if (legal.empty())
    return get_terminal_payoff(state, traverser);

  //
  // Node lookup
  //
  if (std::find(node_map.begin(), node_map.end(), info) != node_map.end())
    node_map[info] = new Node(legal.size());

  Node *node = node_map[info];
  std::vector<double> strategy = node->get_strategy(1.0);

  //
  // -----------------------------------------------------
  //     TRAVERSER — EXPLORE ALL ACTIONS
  // -----------------------------------------------------
  //
  if (acting == traverser) {

    double node_util = 0.0;
    std::vector<double> utils(legal.size());

    for (size_t i = 0; i < legal.size(); ++i) {
      GameState next = state;
      next.apply_action(legal[i]);

      utils[i] = cfr(next, traverser, prob_traverser * strategy[i],
                     prob_opponent, prob_chance, gen, depth + 1);

      node_util += strategy[i] * utils[i];
    }

    //
    // Regret scaled by opponent reach × chance reach
    //
    double scale = prob_opponent * prob_chance;

    for (size_t i = 0; i < legal.size(); ++i) {
      double regret = (utils[i] - node_util) * scale;
      node->update_regret_sum(i, regret);
    }

    return node_util;
  }

  //
  // -----------------------------------------------------
  //     OPPONENT — SAMPLE ONE ACTION only
  // -----------------------------------------------------
  //
  std::discrete_distribution<> dist(strategy.begin(), strategy.end());
  int a = dist(gen);

  GameState next = state;
  next.apply_action(legal[a]);

  return cfr(next, traverser, prob_traverser, prob_opponent * strategy[a],
             prob_chance, gen, depth + 1);
}

//
// ----------------------------------------------
// Strategy exposure
// ----------------------------------------------
//

std::vector<double> Trainer::get_strategy(const std::string &info) {
  if (std::find(node_map.begin(), node_map.end(), info) != node_map.end())
    return node_map[info]->get_average_strategy();
  return {};
}

//
// ----------------------------------------------
// Action recommendation
// ----------------------------------------------
//

Action Trainer::get_action_recommendation(GameState &state, int player_id,
                                          std::vector<double> &probs) {
  std::string info = state.compute_information_set(player_id);
  auto legal = state.get_legal_actions();

  if (legal.empty()) {
    probs.clear();
    return Action(-1, ActionType::FOLD, 0);
  }

  probs = get_strategy(info);
  if (probs.empty())
    probs.assign(legal.size(), 1.0 / legal.size());

  std::random_device rd;
  std::mt19937 gen(rd());
  std::discrete_distribution<> dist(probs.begin(), probs.end());
  int idx = dist(gen) % legal.size();
  return legal[idx];
}

//
// ----------------------------------------------
// Save/Load nodes
// ----------------------------------------------
//

void Trainer::save_to_file(const std::string &fn) {
  std::ofstream out(fn, std::ios::binary);
  if (!out) {
    std::cerr << "Cannot write file " << fn << "\n";
    return;
  }

  size_t N = node_map.size();
  out.write((char *)&N, sizeof(N));

  for (auto &[key, node] : node_map) {
    size_t len = key.size();
    out.write((char *)&len, sizeof(len));
    out.write(key.c_str(), len);

    auto sum = node->get_strategy_sum();
    size_t k = sum.size();
    out.write((char *)&k, sizeof(k));
    out.write((char *)sum.data(), sizeof(double) * k);
  }
}

void Trainer::load_from_file(const std::string &fn) {
  std::ifstream in(fn, std::ios::binary);
  if (!in) {
    std::cerr << "Cannot open file " << fn << "\n";
    return;
  }

  for (auto &[k, n] : node_map)
    delete n;
  node_map.clear();

  size_t N;
  in.read((char *)&N, sizeof(N));

  for (size_t i = 0; i < N; ++i) {
    size_t len;
    in.read((char *)&len, sizeof(len));

    std::string key(len, '\0');
    in.read(&key[0], len);

    size_t k;
    in.read((char *)&k, sizeof(k));

    std::vector<double> sum(k);
    in.read((char *)sum.data(), sizeof(double) * k);

    Node *node = new Node(k);
    node->set_strategy_sum(sum);

    node_map[key] = node;
  }
}
// double Trainer::cfr(GameState &state, int player_id, double history_prob,
//                     std::mt19937 &gen, int depth) {
//   // Removed depth limit to allow full tree exploration
//
//   // Terminal state: return utility
//   if (state.is_terminal()) {
//     Player *p = state.get_player(player_id);
//     if (!p)
//       return 0.0;
//     return get_terminal_payoff(state, player_id);
//     // double utility = p->stack - 1000.0;
//     /// return utility;
//   }
//
//   // Handle betting round transitions
//   if (state.is_betting_round_over() && state.stage != Stage::SHOWDOWN) {
//     // Check if we need to deal cards (chance node)
//     if (state.stage == Stage::PREFLOP && state.community_cards.empty()) {
//       // Deal flop (3 cards)
//       deal_random_community_cards(state, 3, gen);
//       state.stage = Stage::FLOP;
//       state.next_street();
//     } else if (state.stage == Stage::FLOP &&
//                state.community_cards.size() == 3) {
//       // Deal turn (1 card)
//       deal_random_community_cards(state, 1, gen);
//       state.stage = Stage::TURN;
//       state.next_street();
//     } else if (state.stage == Stage::TURN &&
//                state.community_cards.size() == 4) {
//       // Deal river (1 card)
//       deal_random_community_cards(state, 1, gen);
//       state.stage = Stage::RIVER;
//       state.next_street();
//     } else {
//       state.next_street();
//     }
//   }
//
//   // Check terminal again after street transition
//   if (state.is_terminal()) {
//     Player *p = state.get_player(player_id);
//     if (!p)
//       return 0.0;
//     return get_terminal_payoff(state, player_id);
//     // double utility = p->stack - 1000.0;
//     // return utility;
//   }
//
//   Player *curr_player = state.get_current_player();
//   if (!curr_player) {
//     return 0.0;
//   }
//
//   std::string info_set = state.compute_information_set(curr_player->id);
//   std::vector<Action> legal_actions = state.get_legal_actions();
//
//   if (legal_actions.empty())
//     return 0.0;
//
//   if (node_map.find(info_set) == node_map.end()) {
//     node_map[info_set] = new Node(legal_actions.size());
//   }
//   Node *node = node_map[info_set];
//
//   // External Sampling: Update strategy sum only for the traverser
//   (player_id) double weight = (curr_player->id == player_id) ? 1.0 : 0.0;
//   std::vector<double> strategy = node->get_strategy(weight);
//
//   if (strategy.size() != legal_actions.size()) {
//     std::cerr << "Error: Action size mismatch for info_set: " << info_set
//               << "\n";
//     std::cerr << "Node has " << strategy.size()
//               << " actions, but current state has " << legal_actions.size()
//               << "\n";
//     return 0.0;
//   }
//
//   if (curr_player->id == player_id) {
//     // Traverser: Explore ALL actions
//     double node_util = 0.0;
//     std::vector<double> utils(legal_actions.size());
//
//     for (size_t i = 0; i < legal_actions.size(); ++i) {
//       GameState next_state = state;
//       next_state.apply_action(legal_actions[i]);
//
//       utils[i] = cfr(next_state, player_id, history_prob * strategy[i], gen,
//       depth + 1); node_util += strategy[i] * utils[i];
//     }
//
//     // Update Regrets
//     for (size_t i = 0; i < legal_actions.size(); ++i) {
//       double regret = utils[i] - node_util;
//       node->update_regret_sum(i, regret);
//     }
//     return node_util;
//   } else {
//     // Opponent: Sample ONE action
//     std::discrete_distribution<> dist(strategy.begin(), strategy.end());
//     int sampled = dist(gen);
//
//     GameState next_state = state;
//     next_state.apply_action(legal_actions[sampled]);
//
//     return cfr(next_state, player_id, history_prob * strategy[sampled], gen,
//     depth + 1);
//   }
// }
//
// std::vector<double> Trainer::get_strategy(const std::string &info_set) {
//   if (node_map.find(info_set) != node_map.end()) {
//     return node_map[info_set]->get_average_strategy();
//   }
//   return {};
// }
//
// Action Trainer::get_action_recommendation(GameState &state, int player_id,
//                                           std::vector<double> &probabilities)
//                                           {
//   std::string info_set = state.compute_information_set(player_id);
//   std::vector<Action> legal_actions = state.get_legal_actions();
//
//   if (legal_actions.empty()) {
//     probabilities.clear();
//     return Action(-1, ActionType::FOLD, 0);
//   }
//
//   probabilities = get_strategy(info_set);
//
//   if (probabilities.empty()) {
//     probabilities.resize(legal_actions.size(), 1.0 / legal_actions.size());
//   }
//
//   std::random_device rd;
//   std::mt19937 gen(rd());
//   std::discrete_distribution<> dist(probabilities.begin(),
//   probabilities.end()); int selected = dist(gen);
//
//   // solved potential seg fault? making sure idx ix within legal_actions
//   int idx = selected % legal_actions.size();
//   return legal_actions[idx];
// }
//
// void Trainer::save_to_file(const std::string &filename) {
//   std::ofstream out(filename, std::ios::binary);
//   if (!out) {
//     std::cerr << "Failed to open file for writing: " << filename << "\n";
//     return;
//   }
//
//   size_t map_size = node_map.size();
//   out.write(reinterpret_cast<const char *>(&map_size), sizeof(map_size));
//
//   for (const auto &[key, node] : node_map) {
//     size_t key_len = key.size();
//     out.write(reinterpret_cast<const char *>(&key_len), sizeof(key_len));
//     out.write(key.c_str(), key_len);
//
//     // CRITICAL FIX: Save RAW strategy_sum, not normalized average
//     std::vector<double> strat_sum = node->get_strategy_sum();
//     size_t num_actions = strat_sum.size();
//     out.write(reinterpret_cast<const char *>(&num_actions),
//               sizeof(num_actions));
//     out.write(reinterpret_cast<const char *>(strat_sum.data()),
//               num_actions * sizeof(double));
//   }
//
//   out.close();
//   std::cout << "Saved " << map_size << " nodes to " << filename << "\n";
// }
//
// void Trainer::load_from_file(const std::string &filename) {
//   std::ifstream in(filename, std::ios::binary);
//   if (!in) {
//     std::cerr << "Failed to open file for reading: " << filename << "\n";
//     return;
//   }
//
//   for (auto &[key, node] : node_map) {
//     delete node;
//   }
//   node_map.clear();
//
//   size_t map_size;
//   in.read(reinterpret_cast<char *>(&map_size), sizeof(map_size));
//
//   for (size_t i = 0; i < map_size; ++i) {
//     size_t key_len;
//     in.read(reinterpret_cast<char *>(&key_len), sizeof(key_len));
//
//     std::string key(key_len, '\0');
//     in.read(&key[0], key_len);
//
//     size_t num_actions;
//     in.read(reinterpret_cast<char *>(&num_actions), sizeof(num_actions));
//
//     std::vector<double> strat_sum(num_actions);
//     in.read(reinterpret_cast<char *>(strat_sum.data()),
//             num_actions * sizeof(double));
//
//     // Create node and restore the RAW strategy_sum
//     Node *node = new Node(num_actions);
//     node->set_strategy_sum(strat_sum);
//     node_map[key] = node;
//   }
//
//   in.close();
//   std::cout << "Loaded " << map_size << " nodes from " << filename << "\n";
// }
