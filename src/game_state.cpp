#include "../include/game_state.h"
#include <chrono>
#include <iomanip>
#include <iostream>

// player constructor
Player::Player(string _id, double _stack, bool _is_human)
    : id(_id), stack(_stack), current_bet(0), is_folded(false),
      is_all_in(false), is_human(_is_human), times_folded(0), times_raised(0),
      times_called(0), hands_played(0) {
  // init default profile
  profile.hands_observed = 0;
  profile.hands_played = 0;
  profile.hands_voluntarily_entered = 0;
  profile.hands_raised_preflop = 0;
}

// game_state constructor
GameState::GameState(RiskProfiler *rp, EquityModule *em)
    : risk_profiler(rp), equity_module(em) {
  pot_size = 0;
  current_street_highest_bet = 0;
  stage = Stage::START;
  // stage = Stage::PREFLOP;
}

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

  // player picking mode
  int mode;
  cout << "Mode (0: Autonomous, 1: Guided): ";
  cin >> mode;
  play_mode = (mode == 0) ? PlayMode::AUTONOMOUS : PlayMode::GUIDED;

  // get player numbers
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
  players.emplace_back("MYSELF", start_stack, true);
  risk_profiler->add_player("MYSELF", start_stack);

  // init others as bots
  for (int i = 1; i < num_players; i++) {
    string pid = "BOT_" + to_string(i);
    players.emplace_back(pid, start_stack, false);
    risk_profiler->add_player(pid, start_stack);
  }

  dealer_index = 0;
}

void GameState::start_hand() {
  // reset round stats
  pot_size = 0;
  current_street_highest_bet = 0;
  stage = Stage::PREFLOP;
  community_cards.clear();

  // pass "dealer" status down
  dealer_index = (dealer_index + 1) % num_players;

  // clean up deck
  init_deck();
  shuffle_deck();

  // reset for new hands
  risk_profiler->reset_hand();

  cout << "Dealing Starting Hands... \n";
  for (auto &p : players) {
    p.hole_cards.clear();
    p.is_folded = false;
    p.is_all_in = false;
    p.current_bet = 0;

    // deal 2 cards to players
    p.hole_cards.push_back(draw_card());
    p.hole_cards.push_back(draw_card());

    // print your hands
    if (p.is_human) {
      cout << "Your hands are: \n";
      for (auto &c : p.hole_cards) {
        cout << c << ", ";
      }
      cout << "\n";
    }
  }

  // first player assume is to the left of the dealer for simplicity
  current_player_index = (dealer_index + 1) % num_players;
}

void GameState::deal_community_cards() {
  if (stage == Stage::FLOP) {
    // burn
    draw_card();
    community_cards.push_back(draw_card());
    community_cards.push_back(draw_card());
    community_cards.push_back(draw_card());
    cout << "Flop \n";
  } else if (stage == Stage::TURN) {
    // burn
    draw_card();
    community_cards.push_back(draw_card());
    cout << "Turn\n";
  } else if (stage == Stage::RIVER) {
    // burn
    draw_card();
    community_cards.push_back(draw_card());
    cout << "River \n";
  }
}

void GameState::next_street() {
  // logic for the next turn
  // preflop -> flop -> turn -> river -> showdown
  // reset all stats first
  for (auto &p : players) {
    p.current_bet = 0;
  }
  current_street_highest_bet = 0;

  // move player to the left of dealer
  current_player_index = (dealer_index + 1) % num_players;
  while (players[current_player_index].is_folded ||
         players[current_player_index].is_all_in) {
    current_player_index = (current_player_index + 1) % num_players;
  }

  // move to next stage
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

  if (action.type == ActionType::FOLD) {
    p.is_folded = true;
    p.times_folded++;
    cout << p.id << " Fold \n";
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

    cout << p.id << " Call " << call_amount << ".\n";
    risk_profiler->update_stack(p.id, call_amount);
    risk_profiler->update_player_profile(p.id, "call", call_amount, pot_size);
    return true;
  }

  if (action.type == ActionType::RAISE) {
    // amount here is total bet amount for the street
    double actual_raise = action.amount - p.current_bet;
    // ALL IN
    if (action.amount > p.stack + p.current_bet) {
      action.amount = p.stack + p.current_bet;
    }

    p.stack -= actual_raise;
    p.current_bet = action.amount;
    pot_size += actual_raise;

    if (p.stack == 0)
      p.is_all_in = true;
    current_street_highest_bet = action.amount;
    p.times_raised++;

    cout << p.id << " Raises to " << action.amount << ".\n";
    risk_profiler->update_stack(p.id, actual_raise);
    risk_profiler->update_player_profile(p.id, "raise", actual_raise, pot_size);
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
  cout << "\n Showdown: \n";

  // here we use equity_module to evaluate hands
  // Prolly, iterate all non-folded players, run evaluate_hand
  // find max/best hand
  // hand pot to winner

  // placeholder for later integration
  cout << "TODO: find winner using equity_module to evaluate hands \n";
  cout << "TODO: update winner pot \n";
  cout << "TODO: print leaderboard of everyone's gain \n";
}

Action GameState::get_last_action() {
  return history.back();
}