#include "../include/game_state.h"
#include <iomanip>
#include <iostream>

// Player Constructor
Player::Player(int _id, double _stack, bool _is_human)
    : id(_id), stack(_stack), current_bet(0), is_folded(false),
      is_all_in(false), is_human(_is_human), has_acted_this_street(false) {}

// GameState Constructor
GameState::GameState(RiskProfiler *rp, EquityModule *em)
    : risk_profiler(rp), equity_module(em), pot_size(0),
      current_street_highest_bet(0), num_players(0), dealer_index(0),
      current_player_index(0), small_blind_amount(0), big_blind_amount(0),
      stage(Stage::START), type(StateType::CHANCE) {
      for (int r = 0; r < 13; ++r) {
        for (int s = 0; s < 4; ++s) {
          deck.emplace_back(static_cast<Rank>(r), static_cast<Suit>(s));
        }
      }
      }

void GameState::init_game_setup(int n_players, double stack_size, double sb,
                                double bb) {
  num_players = n_players;
  small_blind_amount = sb;
  big_blind_amount = bb;

  players.clear();
  // Player 0 is Human
  players.emplace_back(0, stack_size, true);
  if (risk_profiler)
    risk_profiler->add_player(0, stack_size);

  // Others are Bots
  for (int i = 1; i < num_players; i++) {
    players.emplace_back(i, stack_size, false);
    if (risk_profiler)
      risk_profiler->add_player(i, stack_size);
  }

  dealer_index = 0;
}

void GameState::start_hand() {
  pot_size = 0;
  current_street_highest_bet = 0;
  stage = Stage::START;
  type = StateType::CHANCE; // Waiting for cards
  community_cards.clear();
  history.clear();

  dealer_index = (dealer_index + 1) % num_players;

  if (risk_profiler)
    risk_profiler->reset_hand();

  for (auto &p : players) {
    p.hole_cards.clear();
    p.is_folded = false;
    p.is_all_in = false;
    p.current_bet = 0;
    p.has_acted_this_street = false;
  }

  // Post Blinds
  int sb_pos = (dealer_index + 1) % num_players;
  int bb_pos = (dealer_index + 2) % num_players;

  // SB
  int sb_paid = min(small_blind_amount, players[sb_pos].stack);
  players[sb_pos].stack -= sb_paid;
  players[sb_pos].current_bet = sb_paid;
  pot_size += sb_paid;

  // BB
  int bb_paid = min(big_blind_amount, players[bb_pos].stack);
  players[bb_pos].stack -= bb_paid;
  players[bb_pos].current_bet = bb_paid;
  pot_size += bb_paid;
  current_street_highest_bet = bb_paid;

  // Action starts left of BB
  current_player_index = (bb_pos + 1) % num_players;

  // Advance to PREFLOP immediately after setup
  stage = Stage::PREFLOP;
  type = StateType::PLAY;
}

void GameState::set_community_cards(const std::vector<Card> &cards) {
  community_cards = cards;
}

void GameState::set_player_cards(int player_id,
                                 const std::vector<Card> &cards) {
  if (player_id >= 0 && player_id < (int)players.size()) {
    players[player_id].hole_cards = cards;
  }
}

void GameState::next_street() {
  for (auto &p : players) {
    p.current_bet = 0;
    p.has_acted_this_street = false;
  }
  current_street_highest_bet = 0;

  // Advance stage
  if (stage == Stage::PREFLOP)
    stage = Stage::FLOP;
  else if (stage == Stage::FLOP)
    stage = Stage::TURN;
  else if (stage == Stage::TURN)
    stage = Stage::RIVER;
  else if (stage == Stage::RIVER)
    stage = Stage::SHOWDOWN;

  // Reset action to left of dealer (SB)
  current_player_index = (dealer_index + 1) % num_players;

  // Skip folded/all-in players
  int attempts = 0;
  while ((players[current_player_index].is_folded ||
          players[current_player_index].is_all_in) &&
         attempts < num_players) {
    current_player_index = (current_player_index + 1) % num_players;
    attempts++;
  }
}

bool GameState::record_action(int player_idx, Action action) {
  Player &p = players[player_idx];
  history.push_back(action);
  p.has_acted_this_street = true;

  if (action.type == ActionType::FOLD) {
    p.is_folded = true;
    if (risk_profiler)
      risk_profiler->update_player_profile(p.id, "fold", 0, pot_size);
    return true;
  }

  int amount_added = 0;
  if (action.type == ActionType::CALL) {
    amount_added = min(p.stack, current_street_highest_bet - p.current_bet);
    if (risk_profiler)
      risk_profiler->update_player_profile(p.id, "call", amount_added,
                                           pot_size);
  } else if (action.type == ActionType::BET ||
             action.type == ActionType::RAISE) {
    amount_added = min(p.stack, action.amount - p.current_bet);
    current_street_highest_bet = action.amount;
    if (risk_profiler)
      risk_profiler->update_player_profile(p.id, "raise", amount_added,
                                           pot_size);
  } else if (action.type == ActionType::ALLIN) {
    amount_added = p.stack;
    if (p.current_bet + amount_added > current_street_highest_bet) {
      current_street_highest_bet = p.current_bet + amount_added;
    }
    if (risk_profiler)
      risk_profiler->update_player_profile(p.id, "allin", amount_added,
                                           pot_size);
  } else if (action.type == ActionType::CHECK) {
    return true;
  }

  p.stack -= amount_added;
  p.current_bet += amount_added;
  pot_size += amount_added;

  if (p.stack == 0)
    p.is_all_in = true;

  return true;
}

void GameState::apply_action(Action action) {
  record_action(action.player_id, action);
  determine_next_state();
}

void GameState::determine_next_state() {
  if (is_betting_round_over()) {
    if (stage == Stage::RIVER) {
      type = StateType::TERMINAL;
    } else {
      // Round over, wait for next street cards (handled by main loop calling
      // next_street) But for MCCFR flow, we might need to signal this. In
      // manual mode, main loop controls street transition. So we just stay in
      // PLAY but effectively waiting. Or we can set a flag. Let's assume main
      // loop checks is_betting_round_over()
    }
  } else {
    next_player();
  }

  if (get_active_player_count() <= 1) {
    type = StateType::TERMINAL;
  }
}

bool GameState::is_betting_round_over() {
  int active = 0;
  for (const auto &p : players) {
    if (!p.is_folded && !p.is_all_in) {
      active++;
      if (!p.has_acted_this_street)
        return false;
      if (p.current_bet != current_street_highest_bet)
        return false;
    }
  }
  return true;
}

void GameState::next_player() {
  int attempts = 0;
  do {
    current_player_index = (current_player_index + 1) % num_players;
    attempts++;
  } while ((players[current_player_index].is_folded ||
            players[current_player_index].is_all_in) &&
           attempts <= num_players);
}

int GameState::get_active_player_count() {
  int count = 0;
  for (const auto &p : players)
    if (!p.is_folded)
      count++;
  return count;
}

Player *GameState::get_current_player() {
  return &players[current_player_index];
}

Player *GameState::get_player(int player_id) {
  for (auto &p : players)
    if (p.id == player_id)
      return &p;
  return nullptr;
}

bool GameState::is_terminal() { return type == StateType::TERMINAL; }

bool GameState::is_chance() { return type == StateType::CHANCE; }

// MCCFR Information Set
string GameState::compute_information_set(int player_id) {
  std::string info;
  Player *p = get_player(player_id);
  if (!p)
    return "INVALID";

  street st = PRE;
  if (stage == Stage::FLOP)
    st = FLOP;
  else if (stage == Stage::TURN)
    st = TURN;
  else if (stage == Stage::RIVER)
    st = RIVER;

  int bucket = 0;
  if (equity_module && p->hole_cards.size() >= 2) {
    bucket = equity_module->bucketize_hand(p->hole_cards, community_cards, st);
  }

  info += std::to_string(bucket) + "|";
  info += std::to_string((int)stage) + "|";

  // ABSTRACTION: Normalize to big blinds
  double bb = big_blind_amount;
  if (bb <= 0)
    bb = 1.0; // Safety check

  // Abstract stack size to buckets
  double stack_bb = p->stack / bb;
  int stack_bucket = abstract_stack_size(stack_bb);
  info += std::to_string(stack_bucket) + "|";

  // Abstract pot size to buckets
  double pot_bb = pot_size / bb;
  int pot_bucket = abstract_pot_size(pot_bb);
  info += std::to_string(pot_bucket) + "|";

  // Abstract action history with bet sizing
  info += abstract_action_history() + "|";

  std::vector<Action> legal = get_legal_actions();
  info += std::to_string(legal.size());

  return info;
}

void GameState::remove_from_deck(Card c) {
  if (std::find(deck.begin(), deck.end(), c) != deck.end()) {
      deck.erase(std::remove(deck.begin(), deck.end(), c), deck.end());
  }
}

std::vector<Card> GameState::get_remaining_deck() {
  std::vector<Card> rem = deck;
  for (int i = 0; i < num_players; ++i) {
    Player* p = get_player(i);
    for (Card c : p->hole_cards) {
      if (std::find(rem.begin(), rem.end(), c) != rem.end()) {
        rem.erase(std::remove(rem.begin(), rem.end(), c), rem.end());
      }
    }
  }
  for (Card c : community_cards) {
    if (std::find(rem.begin(), rem.end(), c) != rem.end()) {
      rem.erase(std::remove(rem.begin(), rem.end(), c), rem.end());
    }
  } 

  return rem;
}

std::vector<pair<GameState, double>> GameState::get_chance_outcomes(GameState &state) {
      vector<pair<GameState, double>> outcomes;

    if (state.stage == Stage::PREFLOP) {
        std::vector<Card> rem = state.get_remaining_deck();
        int n = rem.size();
        for (int i = 0; i < n; i++) {
            for (int j = i+1; j < n; j++) {
                for (int k = j+1; k < n; k++) {
                    GameState next = state;
                    next.community_cards.push_back(rem[i]);
                    next.community_cards.push_back(rem[j]);
                    next.community_cards.push_back(rem[k]);
                    next.stage = Stage::FLOP;
                    next.remove_from_deck(rem[i]);
                    next.remove_from_deck(rem[j]);
                    next.remove_from_deck(rem[k]);

                    outcomes.emplace_back(next, 1.0);
                }
            }
        }

        if (outcomes.empty()) return outcomes;

        // normalize probability
        double p = 1.0 / outcomes.size();
        for (auto &o : outcomes) o.second = p;

        return outcomes;
    }

    if (state.stage == Stage::FLOP) {
        std::vector<Card> rem = state.get_remaining_deck();
        double p = 1.0 / (int) rem.size();

        for (Card c : rem) {
            GameState next = state;
            next.community_cards.push_back(c);
            next.stage = Stage::TURN;
            next.remove_from_deck(c);

            outcomes.emplace_back(next, p);
        }
        return outcomes;
    }

    if (state.stage == Stage::TURN) {
        std::vector<Card> rem = state.get_remaining_deck();
        double p = 1.0 / (int) rem.size();

        for (Card c : rem) {
            GameState next = state;
            next.community_cards.push_back(c);
            next.stage = Stage::RIVER;
            next.remove_from_deck(c);

            outcomes.emplace_back(next, p);
        }
        return outcomes;
    }

    return outcomes;
}

// Abstract stack into buckets based on big blinds
int GameState::abstract_stack_size(double stack_bb) const {
  if (stack_bb < 10)
    return 0; // Short stack
  if (stack_bb < 25)
    return 1; // Medium-short
  if (stack_bb < 50)
    return 2; // Medium
  if (stack_bb < 100)
    return 3; // Medium-deep
  return 4;   // Deep stack
}

// Abstract pot into buckets
int GameState::abstract_pot_size(double pot_bb) const {
  if (pot_bb < 5)
    return 0; // Small pot
  if (pot_bb < 15)
    return 1; // Medium pot
  if (pot_bb < 30)
    return 2; // Large pot
  if (pot_bb < 60)
    return 3; // Very large pot
  return 4;   // Huge pot
}

// Abstract bet size relative to pot
std::string GameState::abstract_bet_size(double bet_amount) const {
  if (pot_size <= 0)
    return "S"; // If no pot, default to small

  double pot_fraction = bet_amount / pot_size;

  if (pot_fraction < 0.4)
    return "S"; // Small (< 1/3 pot)
  if (pot_fraction < 0.75)
    return "M"; // Medium (1/2 pot)
  if (pot_fraction < 1.5)
    return "P"; // Pot-sized
  if (pot_fraction < 2.5)
    return "L"; // Large (2x pot)
  return "A";   // All-in / Overbet
}

// Simplify action history with abstracted bet sizes and RELATIVE positions
std::string GameState::abstract_action_history() const {
  std::string result;
  for (const auto &action : history) {
    // Calculate relative position: (action_player - dealer + num_players) %
    // num_players This makes 0=Dealer, 1=SB, 2=BB, etc. regardless of absolute
    // ID
    int relative_pos =
        (action.player_id - dealer_index + num_players) % num_players;

    result += std::to_string(relative_pos);

    switch (action.type) {
    case ActionType::FOLD:
      result += "F";
      break;
    case ActionType::CHECK:
      result += "X";
      break;
    case ActionType::CALL:
      result += "C";
      break;
    case ActionType::BET:
    case ActionType::RAISE:
      result += "R" + abstract_bet_size(action.amount);
      break;
    case ActionType::ALLIN:
      result += "A";
      break;
    }
  }
  return result.empty() ? "_" : result;
}

std::vector<Action> GameState::get_legal_actions() {
  std::vector<Action> actions;
  if (is_terminal())
    return actions;

  Player *p = get_current_player();
  int call_amt = current_street_highest_bet - p->current_bet;

  actions.emplace_back(p->id, ActionType::FOLD, 0);

  if (call_amt == 0) {
    actions.emplace_back(p->id, ActionType::CHECK, 0);
    // Simplified betting sizes for MCCFR
    double pot = std::max(10.0, pot_size);
    if (p->stack >= pot / 2.0)
      actions.emplace_back(p->id, ActionType::BET, pot / 2.0);
    if (p->stack >= pot)
      actions.emplace_back(p->id, ActionType::BET, pot);
    actions.emplace_back(p->id, ActionType::ALLIN, p->stack);
  } else {
    if (p->stack > call_amt) {
      actions.emplace_back(p->id, ActionType::CALL, call_amt);
      double pot = std::max(call_amt + 10.0, pot_size);
      if (p->stack >= pot)
        actions.emplace_back(p->id, ActionType::RAISE, pot);
      actions.emplace_back(p->id, ActionType::ALLIN, p->stack);
    } else {
      actions.emplace_back(p->id, ActionType::CALL, p->stack);
    }
  }
  return actions;
}

void GameState::resolve_winner() {
  // Manual resolution or simple equity check
  // For now, simple equity check if we have cards
  int winner_id = -1;
  int best_score = -1;

  for (auto &p : players) {
    if (p.is_folded)
      continue;

    // If we don't have opponent cards (manual mode), we can't determine winner
    // automatically unless we ask user. For now, let's just print "Showdown"
    if (p.hole_cards.empty())
      continue;

    std::vector<Card> hand = p.hole_cards;
    hand.insert(hand.end(), community_cards.begin(), community_cards.end());
    int score = equity_module->evaluate_7_cards(hand);
    if (score > best_score) {
      best_score = score;
      winner_id = p.id;
    }
  }

  if (winner_id != -1) {
    cout << "Winner determined by cards: Player " << winner_id << "\n";
  } else {
    cout << "Showdown! (Enter winner manually in next version)\n";
  }
}
