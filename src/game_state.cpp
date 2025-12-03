#include "../include/game_state.h"
#include <chrono>
#include <iomanip>
#include <iostream>

// player constructor
Player::Player(int _id, double _stack, bool _is_human)
    : id(_id), stack(_stack), current_bet(0), is_folded(false),
      is_all_in(false), is_human(_is_human), times_folded(0), times_raised(0),
      times_called(0), hands_played(0), has_acted_this_street(false) {
  // init default profile
  profile.hands_observed = 0;
  profile.hands_played = 0;
  profile.hands_voluntarily_entered = 0;
  profile.hands_raised_preflop = 0;
}

// game_state constructor
GameState::GameState(RiskProfiler *rp, EquityModule *em)
    : risk_profiler(rp), equity_module(em), pot_size(0),
      current_street_highest_bet(0), num_players(0), dealer_index(0),
      current_player_index(0), small_blind_pos(0), big_blind_pos(0),
      small_blind_amount(0), big_blind_amount(0), stage(Stage::START) {}

void GameState::init_deck() {
  deck.clear();
  // Rank 0-12, Suit 0-3
  for (int s = 0; s < 4; s++) {
    for (int r = 0; r < 13; r++) {
      deck.emplace_back(static_cast<Rank>(r), static_cast<Suit>(s));
    }
  }
}
// should work?
void GameState::shuffle_deck() {
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::shuffle(deck.begin(), deck.end(), std::default_random_engine(seed));
}

Card GameState::draw_card() {
  if (deck.empty()) {
    // this shouldn't have happened, but just in case if it does again
    return Card(Rank::TWO, Suit::CLUBS);
  }
  Card c = deck.back();
  deck.pop_back();
  return c;
}

void GameState::init_game_setup() {
  cout << "Poker Engine Initialization \n";

  cout << "Enter Total Players (Including Yourself): ";
  cin >> num_players;
  if (num_players < 2) {
    num_players = 2;
  }

  // init stack
  int start_stack;
  cout << "Enter Starting Stack: ";
  cin >> start_stack;

  players.clear();

  // init myself
  players.emplace_back(0, start_stack, true);
  risk_profiler->add_player(0, start_stack);

  // init others as bots
  for (int i = 1; i < num_players; i++) {
    players.emplace_back(i, start_stack, false);
    risk_profiler->add_player(i, start_stack);
  }

  dealer_index = 0;
}

void GameState::start_hand() {
  // reset round stats
  pot_size = 0;
  current_street_highest_bet = 0;
  stage = Stage::START;     // Start stage
  type = StateType::CHANCE; // Initial state is CHANCE (to deal hole cards)
  community_cards.clear();
  history.clear();

  // pass "dealer" status down
  dealer_index = (dealer_index + 1) % num_players;

  // clean up deck
  init_deck();
  shuffle_deck();

  // reset for new hands
  risk_profiler->reset_hand();

  cout << "Starting Hand... (State: CHANCE)\n";
  for (auto &p : players) {
    p.hole_cards.clear();
    p.is_folded = false;
    p.is_all_in = false;
    p.current_bet = 0;
    p.has_acted_this_street = false;
  }

  // first player assume is to the left of the dealer for simplicity
  // But actually, for CHANCE state, current_player_index doesn't matter much,
  // but let's set it to SB for when PLAY starts.
  // Actually, Preflop starts with UTG (or SB/BB posting).
  // Let's set it to SB pos for now.
  current_player_index = (dealer_index + 1) % num_players;
}

void GameState::deal_community_cards() {
  if (stage == Stage::FLOP) {
    draw_card();
    community_cards.push_back(draw_card());
    community_cards.push_back(draw_card());
    community_cards.push_back(draw_card());
  } else if (stage == Stage::TURN) {
    draw_card();
    community_cards.push_back(draw_card());
  } else if (stage == Stage::RIVER) {
    draw_card();
    community_cards.push_back(draw_card());
  }
}

void GameState::next_street() {
  for (auto &p : players) {
    p.current_bet = 0;
    p.has_acted_this_street = false;
  }
  current_street_highest_bet = 0;

  int active_count = 0;
  for (const auto &p : players) {
    if (!p.is_folded && !p.is_all_in) {
      active_count++;
    }
  }

  if (active_count == 0) {
    stage = Stage::SHOWDOWN;
    return;
  }

  current_player_index = (dealer_index + 1) % num_players;
  int attempts = 0;
  while ((players[current_player_index].is_folded ||
          players[current_player_index].is_all_in) &&
         attempts < num_players) {
    current_player_index = (current_player_index + 1) % num_players;
    attempts++;
  }

  if (stage == Stage::START)
    stage = Stage::PREFLOP;
  else if (stage == Stage::PREFLOP)
    stage = Stage::FLOP;
  else if (stage == Stage::FLOP)
    stage = Stage::TURN;
  else if (stage == Stage::TURN)
    stage = Stage::RIVER;
  else if (stage == Stage::RIVER)
    stage = Stage::SHOWDOWN;

  deal_community_cards();
}

bool GameState::record_action(int player_idx, Action action) {
  // HELP: confused, are playerid and playeridx different?
  Player &p = players[player_idx];

  history.push_back(action);
  p.has_acted_this_street = true;

  if (action.type == ActionType::FOLD) {
    p.is_folded = true;
    p.times_folded++;
    if (risk_profiler)
      risk_profiler->update_player_profile(p.id, "fold", 0, pot_size);
    return true;
  }

  if (action.type == ActionType::CALL) {
    double call_amount = current_street_highest_bet - p.current_bet;
    if (call_amount > p.stack)
      call_amount = p.stack; // all-in call

    p.stack -= call_amount;
    p.current_bet += call_amount;
    pot_size += call_amount;

    if (p.stack == 0) {
      p.is_all_in = true;
    }

    p.times_called++;

    if (risk_profiler) {
      risk_profiler->update_stack(p.id, call_amount);
      risk_profiler->update_player_profile(p.id, "call", call_amount, pot_size);
    }
    return true;
  }

  if (action.type == ActionType::BET) {
    double actual_bet = action.amount - p.current_bet;
    if (action.amount > p.stack + p.current_bet) {
      action.amount = p.stack + p.current_bet;
      actual_bet = p.stack;
    }

    p.stack -= actual_bet;
    p.current_bet = action.amount;
    pot_size += actual_bet;

    if (p.stack == 0)
      p.is_all_in = true;
    current_street_highest_bet = action.amount;
    p.times_raised++;

    if (risk_profiler) {
      risk_profiler->update_stack(p.id, actual_bet);
      risk_profiler->update_player_profile(p.id, "bet", actual_bet, pot_size);
    }
    return true;
  }

  if (action.type == ActionType::RAISE) {
    double actual_raise = action.amount - p.current_bet;
    if (action.amount > p.stack + p.current_bet) {
      action.amount = p.stack + p.current_bet;
      actual_raise = p.stack;
    }

    p.stack -= actual_raise;
    p.current_bet = action.amount;
    pot_size += actual_raise;

    if (p.stack == 0)
      p.is_all_in = true;
    current_street_highest_bet = action.amount;
    p.times_raised++;

    if (risk_profiler) {
      risk_profiler->update_stack(p.id, actual_raise);
      risk_profiler->update_player_profile(p.id, "raise", actual_raise,
                                           pot_size);
    }
    return true;
  }

  if (action.type == ActionType::CHECK) {
    return true;
  }

  if (action.type == ActionType::ALLIN) {
    int all_in_amount = p.stack;
    p.stack = 0;
    p.current_bet += all_in_amount;
    pot_size += all_in_amount;
    p.is_all_in = true;

    if (p.current_bet > current_street_highest_bet)
      current_street_highest_bet = p.current_bet;

    if (risk_profiler) {
      risk_profiler->update_stack(p.id, all_in_amount);
      risk_profiler->update_player_profile(p.id, "allin", all_in_amount,
                                           pot_size);
    }
    return true;
  }

  return false;
}
// utility func for debugging
Player *GameState::get_current_player() {
  return &players[current_player_index];
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

void GameState::apply_chance(const std::vector<Card> &manual_cards) {
  if (type != StateType::CHANCE)
    return;

  if (stage == Stage::START) {
    // Deal Hole Cards
    // If manual cards provided, use them for the human player (or all if
    // specified) For now, let's assume manual_cards contains cards for the
    // human player if provided. But we need to deal to ALL players.

    // If manual_cards is empty, deal random to everyone.
    // If manual_cards has cards, assign to human, deal random to others.

    // Find human player
    int human_idx = -1;
    for (auto &p : players)
      if (p.is_human)
        human_idx = p.id;

    for (auto &p : players) {
      if (p.is_human && !manual_cards.empty() && manual_cards.size() >= 2) {
        p.hole_cards.push_back(manual_cards[0]);
        p.hole_cards.push_back(manual_cards[1]);
      } else {
        p.hole_cards.push_back(draw_card());
        p.hole_cards.push_back(draw_card());
      }
    }

    stage = Stage::PREFLOP;
    type = StateType::PLAY;

    // Post blinds
    int sb_pos = small_blind_pos;
    int bb_pos = big_blind_pos;

    // SB
    int sb_paid = min(small_blind_amount, players[sb_pos].stack);
    players[sb_pos].stack -= sb_paid;
    players[sb_pos].current_bet = sb_paid;
    pot_size += sb_paid;
    if (players[sb_pos].stack == 0)
      players[sb_pos].is_all_in = true;

    // BB
    int bb_paid = min(big_blind_amount, players[bb_pos].stack);
    players[bb_pos].stack -= bb_paid;
    players[bb_pos].current_bet = bb_paid;
    pot_size += bb_paid;
    current_street_highest_bet = bb_paid;
    if (players[bb_pos].stack == 0)
      players[bb_pos].is_all_in = true;

    // Reset has_acted for blinds (they act last in preflop usually, or first if
    // heads up? Standard: BB acts last preflop. SB acts before BB. But preflop
    // starts left of BB.
    players[sb_pos].has_acted_this_street = false;
    players[bb_pos].has_acted_this_street = false;

    // Set current player to left of BB
    current_player_index = (big_blind_pos + 1) % num_players;

  } else if (stage == Stage::PREFLOP) {
    // Deal Flop
    if (!manual_cards.empty() && manual_cards.size() >= 3) {
      community_cards.push_back(manual_cards[0]);
      community_cards.push_back(manual_cards[1]);
      community_cards.push_back(manual_cards[2]);
    } else {
      draw_card(); // burn
      community_cards.push_back(draw_card());
      community_cards.push_back(draw_card());
      community_cards.push_back(draw_card());
    }
    stage = Stage::FLOP;
    type = StateType::PLAY;
    next_street(); // Reset bets
  } else if (stage == Stage::FLOP) {
    // Deal Turn
    if (!manual_cards.empty() && manual_cards.size() >= 1) {
      community_cards.push_back(manual_cards[0]);
    } else {
      draw_card(); // burn
      community_cards.push_back(draw_card());
    }
    stage = Stage::TURN;
    type = StateType::PLAY;
    next_street();
  } else if (stage == Stage::TURN) {
    // Deal River
    if (!manual_cards.empty() && manual_cards.size() >= 1) {
      community_cards.push_back(manual_cards[0]);
    } else {
      draw_card(); // burn
      community_cards.push_back(draw_card());
    }
    stage = Stage::RIVER;
    type = StateType::PLAY;
    next_street();
  }
}

void GameState::determine_next_state() {
  if (is_betting_round_over()) {
    if (stage == Stage::RIVER) {
      type = StateType::TERMINAL;
    } else {
      type = StateType::CHANCE;
      // Don't advance stage here, apply_chance will do it.
      // But we need to know we are waiting for next street cards.
    }
  } else {
    type = StateType::PLAY;
    next_player();
  }

  // Check if only one player left (everyone else folded)
  int active_count = 0;
  for (auto &p : players)
    if (!p.is_folded)
      active_count++;
  if (active_count <= 1) {
    type = StateType::TERMINAL;
  }
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

    int third_pot = max(10, pot_size / 3);
    int half_pot = max(10, pot_size / 2);
    int pot = max(10, pot_size);

    if (p->stack >= third_pot)
      actions.emplace_back(p->id, ActionType::BET, third_pot);
    if (p->stack >= half_pot && half_pot != third_pot)
      actions.emplace_back(p->id, ActionType::BET, half_pot);
    if (p->stack >= pot && pot != half_pot)
      actions.emplace_back(p->id, ActionType::BET, pot);
    if (p->stack > pot)
      actions.emplace_back(p->id, ActionType::ALLIN, p->stack);
  } else {
    if (p->stack > call_amt) {
      actions.emplace_back(p->id, ActionType::CALL, call_amt);

      int third_pot = max(call_amt + 10, pot_size / 3);
      int half_pot = max(call_amt + 10, pot_size / 2);
      int pot = max(call_amt + 10, pot_size);

      if (p->stack >= third_pot)
        actions.emplace_back(p->id, ActionType::RAISE, third_pot);
      if (p->stack >= half_pot && half_pot != third_pot)
        actions.emplace_back(p->id, ActionType::RAISE, half_pot);
      if (p->stack >= pot && pot != half_pot)
        actions.emplace_back(p->id, ActionType::RAISE, pot);
      if (p->stack > pot)
        actions.emplace_back(p->id, ActionType::ALLIN, p->stack);
    } else {
      actions.emplace_back(p->id, ActionType::CALL, p->stack);
    }
  }
  return actions;
}

void GameState::apply_action(Action action) {
  record_action(action.player_id, action);
  determine_next_state();
}

bool GameState::is_terminal() { return type == StateType::TERMINAL; }

// logic loop in main.cpp instead
// bool GameState::is_betting_round_over() {
//     // round is over if all active players have matched the highest bet
//     // or only one player remains
//     int active = 0;
//     int matches = 0;

//     for(const auto& p : players) {
//         if (!p.is_folded && !p.is_all_in) {
//             active++;
//             if (p.current_bet == current_street_highest_bet &&
//             current_street_highest_bet > 0) {
//                 matches++;
//             }
//             // when everyone folds, let's not deal with this please
//             if (current_street_highest_bet == 0) {
//                 // technically we should track if everyone fold/raise/etc...
//                 // but let's assume this can't happen
//             }
//         }
//     }
//     // everyone else folds
//     if (get_active_player_count() == 1) {
//         return true;
//     }

//     // in poker, need to check two things:
//     // if bets match
//     // if all players had a chance to act on last raise
//     //
//     return false;
// }

int GameState::get_active_player_count() {
  int count = 0;
  for (const auto &p : players) {
    if (!p.is_folded)
      count++;
  }
  return count;
}

void GameState::resolve_winner() {
  cout << "\nShowdown:\n";

  int best_score = -1;
  int winner_id = -1;

  for (auto &p : players) {
    if (p.is_folded)
      continue;

    std::vector<Card> hand = p.hole_cards;
    hand.insert(hand.end(), community_cards.begin(), community_cards.end());

    int score = equity_module->evaluate_7_cards(hand);

    if (score > best_score) {
      best_score = score;
      winner_id = p.id;
    }
  }

  if (winner_id >= 0) {
    Player *winner = get_player(winner_id);
    winner->stack += pot_size;
    cout << "Player " << winner_id << " wins pot of " << pot_size << "!\n";
    cout << "Final stack: " << winner->stack << "\n";
  }
}

Action GameState::get_last_action() { return history.back(); }

Player *GameState::get_player(int player_id) {
  for (auto &p : players) {
    if (p.id == player_id)
      return &p;
  }
  return nullptr;
}

bool GameState::is_betting_round_over() {
  for (const auto &p : players) {
    if (!p.is_folded && !p.is_all_in) {
      cout << "[DEBUG] P" << p.id << " acted=" << p.has_acted_this_street
           << " bet=" << p.current_bet << " high=" << current_street_highest_bet
           << "\n";
      if (!p.has_acted_this_street)
        return false;
      if (p.current_bet != current_street_highest_bet)
        return false;
    }
  }
  return true;
}

// Removed duplicate definitions of is_terminal and apply_action
// They are now defined earlier in the file.

string GameState::compute_information_set(int player_id) {
  std::string info;
  Player *p = get_player(player_id);

  if (!p) {
    return "INVALID_PLAYER";
  }

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

  int pot_bucket = pot_size / 50;
  int stack_bucket = p->stack / 100;
  info += std::to_string(pot_bucket) + "|";
  info += std::to_string(stack_bucket) + "|";

  for (auto &a : history) {
    info +=
        std::to_string(a.player_id) + ":" + std::to_string((int)a.type) + ";";
  }

  // Append number of legal actions to prevent collisions
  std::vector<Action> legal = get_legal_actions();
  info += "|" + std::to_string(legal.size());

  return info;
}
