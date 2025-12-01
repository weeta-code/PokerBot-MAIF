#include "../include/game_state.h"
#include "../include/mccfr/trainer.h"
#include <iostream>

using namespace std;

Card parse_card(const string &s) {
  if (s.length() != 2)
    return Card(Rank::TWO, Suit::CLUBS);

  Rank r;
  switch (s[0]) {
  case '2':
    r = Rank::TWO;
    break;
  case '3':
    r = Rank::THREE;
    break;
  case '4':
    r = Rank::FOUR;
    break;
  case '5':
    r = Rank::FIVE;
    break;
  case '6':
    r = Rank::SIX;
    break;
  case '7':
    r = Rank::SEVEN;
    break;
  case '8':
    r = Rank::EIGHT;
    break;
  case '9':
    r = Rank::NINE;
    break;
  case 'T':
    r = Rank::TEN;
    break;
  case 'J':
    r = Rank::JACK;
    break;
  case 'Q':
    r = Rank::QUEEN;
    break;
  case 'K':
    r = Rank::KING;
    break;
  case 'A':
    r = Rank::ACE;
    break;
  default:
    r = Rank::TWO;
  }

  Suit suit;
  switch (s[1]) {
  case 'c':
    suit = Suit::CLUBS;
    break;
  case 'd':
    suit = Suit::DIAMONDS;
    break;
  case 'h':
    suit = Suit::HEARTS;
    break;
  case 's':
    suit = Suit::SPADES;
    break;
  default:
    suit = Suit::CLUBS;
  }

  return Card(r, suit);
}

int next_active_player(GameState &game, int start) {
  int idx = start;
  int attempts = 0;
  do {
    idx = (idx + 1) % game.num_players;
    attempts++;
    if (attempts > game.num_players)
      return -1;
  } while (game.players[idx].is_folded || game.players[idx].is_all_in);
  return idx;
}

bool betting_round_complete(GameState &game) {
  int active_count = 0;
  int highest_bet = game.current_street_highest_bet;

  for (auto &p : game.players) {
    if (!p.is_folded && !p.is_all_in) {
      active_count++;
      if (p.current_bet != highest_bet)
        return false;
    }
  }

  return active_count <= 1 || (active_count > 0 && highest_bet > 0);
}

void unified_play_mode(Trainer &trainer) {
  RiskProfiler rp;
  EquityModule em;
  GameState game(&rp, &em);

  int num_players, sb_amount, bb_amount;
  cout << "Number of players: ";
  cin >> num_players;
  cin.ignore(10000, '\n');
  cout << "Small blind amount: ";
  cin >> sb_amount;
  cin.ignore(10000, '\n');
  cout << "Big blind amount: ";
  cin >> bb_amount;
  cin.ignore(10000, '\n');

  game.num_players = num_players;
  game.players.clear();

  for (int i = 0; i < num_players; ++i) {
    int stack;
    cout << "Player " << i << " stack: ";
    cin >> stack;
    cin.ignore(10000, '\n');
    game.players.emplace_back(i, stack, false);
    rp.add_player(i, stack);
  }

  int user_pos;
  cout << "Which player are you (0-" << (num_players - 1) << "): ";
  cin >> user_pos;
  cin.ignore(10000, '\n');
  game.players[user_pos].is_human = true;

  int dealer_pos;
  cout << "Dealer button position: ";
  cin >> dealer_pos;
  cin.ignore(10000, '\n');

  game.dealer_index = dealer_pos;
  int sb_pos = (dealer_pos + 1) % num_players;
  int bb_pos = (dealer_pos + 2) % num_players;

  game.stage = Stage::PREFLOP;
  game.pot_size = 0;
  game.current_street_highest_bet = 0;
  game.community_cards.clear();
  game.history.clear();

  for (int i = 0; i < num_players; ++i) {
    game.players[i].hole_cards.clear();
    game.players[i].is_folded = false;
    game.players[i].is_all_in = false;
    game.players[i].current_bet = 0;
  }

  string c1, c2;
  cout << "Your hole cards (e.g., Ah Kd): ";
  cin >> c1 >> c2;
  game.players[user_pos].hole_cards.push_back(parse_card(c1));
  game.players[user_pos].hole_cards.push_back(parse_card(c2));

  int sb_paid = min(sb_amount, game.players[sb_pos].stack);
  game.players[sb_pos].stack -= sb_paid;
  game.players[sb_pos].current_bet = sb_paid;
  game.pot_size += sb_paid;
  if (game.players[sb_pos].stack == 0)
    game.players[sb_pos].is_all_in = true;

  int bb_paid = min(bb_amount, game.players[bb_pos].stack);
  game.players[bb_pos].stack -= bb_paid;
  game.players[bb_pos].current_bet = bb_paid;
  game.pot_size += bb_paid;
  game.current_street_highest_bet = bb_paid;
  if (game.players[bb_pos].stack == 0)
    game.players[bb_pos].is_all_in = true;

  game.current_player_index = (bb_pos + 1) % num_players;
  while (game.players[game.current_player_index].is_folded ||
         game.players[game.current_player_index].is_all_in) {
    game.current_player_index = (game.current_player_index + 1) % num_players;
  }

  while (!game.is_terminal()) {
    while (!betting_round_complete(game) &&
           game.get_active_player_count() > 1) {
      Player *p = game.get_current_player();

      cout << "\n========================================\n";
      cout << "Stage: ";
      switch (game.stage) {
      case Stage::PREFLOP:
        cout << "PREFLOP";
        break;
      case Stage::FLOP:
        cout << "FLOP";
        break;
      case Stage::TURN:
        cout << "TURN";
        break;
      case Stage::RIVER:
        cout << "RIVER";
        break;
      default:
        cout << "SHOWDOWN";
      }
      cout << " | Pot: " << game.pot_size << "\n";
      cout << "Board: ";
      for (auto &c : game.community_cards)
        cout << c << " ";
      cout << "\n";

      cout << "\nCurrent Player: " << p->id << " (Stack: " << p->stack << ")\n";
      if (p->is_human) {
        cout << "Your Cards: " << p->hole_cards[0] << " " << p->hole_cards[1]
             << "\n";
      } else {
        cout << "Opponent (cards hidden)\n";
      }
      cout << "Current Bet: " << p->current_bet << " | To Call: "
           << (game.current_street_highest_bet - p->current_bet) << "\n";

      Action selected(-1, ActionType::FOLD, 0);

      if (p->is_human) {
        vector<double> probs;
        Action recommended =
            trainer.get_action_recommendation(game, p->id, probs);
        vector<Action> legal = game.get_legal_actions();

        cout << "\n--- MCCFR Recommendation ---\n";
        for (size_t i = 0; i < legal.size() && i < probs.size(); ++i) {
          cout << i << ": ";
          switch (legal[i].type) {
          case ActionType::FOLD:
            cout << "FOLD";
            break;
          case ActionType::CHECK:
            cout << "CHECK";
            break;
          case ActionType::CALL:
            cout << "CALL " << legal[i].amount;
            break;
          case ActionType::BET:
            cout << "BET " << legal[i].amount;
            break;
          case ActionType::RAISE:
            cout << "RAISE " << legal[i].amount;
            break;
          case ActionType::ALLIN:
            cout << "ALLIN " << legal[i].amount;
            break;
          }
          cout << " [" << (probs[i] * 100) << "%]\n";
        }

        int choice;
        cout << "\nEnter action index (-1 for recommendation): ";
        cin >> choice;
        cin.ignore(10000, '\n');

        selected = (choice == -1 || choice < 0 || choice >= (int)legal.size())
                       ? recommended
                       : legal[choice];
      } else {
        int call_amt = game.current_street_highest_bet - p->current_bet;

        cout << "\nWhat did Player " << p->id << " do?\n";
        cout << "1. FOLD\n";
        if (call_amt == 0) {
          cout << "2. CHECK\n";
          cout << "3. BET\n";
        } else {
          cout << "2. CALL\n";
          cout << "3. RAISE\n";
        }
        cout << "4. ALL-IN\n";
        cout << "Select: ";

        int action;
        cin >> action;
        cin.ignore(10000, '\n');

        if (action == 1) {
          selected = Action(p->id, ActionType::FOLD, 0);
        } else if (action == 2) {
          if (call_amt == 0) {
            selected = Action(p->id, ActionType::CHECK, 0);
          } else {
            selected = Action(p->id, ActionType::CALL, call_amt);
          }
        } else if (action == 3) {
          double amount;
          if (call_amt == 0) {
            cout << "Bet amount: ";
            cin >> amount;
            cin.ignore(10000, '\n');
            selected = Action(p->id, ActionType::BET, (int)amount);
          } else {
            cout << "Raise to amount: ";
            cin >> amount;
            cin.ignore(10000, '\n');
            selected = Action(p->id, ActionType::RAISE, (int)amount);
          }
        } else if (action == 4) {
          selected = Action(p->id, ActionType::ALLIN, p->stack);
        } else {
          selected = Action(p->id, ActionType::FOLD, 0);
        }
      }

      game.apply_action(selected);

      int next = next_active_player(game, game.current_player_index - 1);
      if (next == -1)
        break;
      game.current_player_index = next;
    }

    if (game.get_active_player_count() <= 1)
      break;

    if (game.stage == Stage::PREFLOP) {
      cout << "\nDealing FLOP...\n";
      string f1, f2, f3;
      cout << "Enter flop cards: ";
      cin >> f1 >> f2 >> f3;
      game.community_cards.push_back(parse_card(f1));
      game.community_cards.push_back(parse_card(f2));
      game.community_cards.push_back(parse_card(f3));
      game.stage = Stage::FLOP;
    } else if (game.stage == Stage::FLOP) {
      cout << "\nDealing TURN...\n";
      string t;
      cout << "Enter turn card: ";
      cin >> t;
      game.community_cards.push_back(parse_card(t));
      game.stage = Stage::TURN;
    } else if (game.stage == Stage::TURN) {
      cout << "\nDealing RIVER...\n";
      string r;
      cout << "Enter river card: ";
      cin >> r;
      game.community_cards.push_back(parse_card(r));
      game.stage = Stage::RIVER;
    } else {
      game.stage = Stage::SHOWDOWN;
      break;
    }

    for (auto &p : game.players) {
      p.current_bet = 0;
    }
    game.current_street_highest_bet = 0;

    int first = (sb_pos);
    while (game.players[first].is_folded || game.players[first].is_all_in) {
      first = (first + 1) % num_players;
    }
    game.current_player_index = first;
  }

  game.resolve_winner();
}

void train_mccfr(Trainer &trainer) {
  int iterations;
  cout << "Enter training iterations: ";
  cin >> iterations;
  trainer.train(iterations);
  cout << "Training complete.\n";
}

int main() {
  RiskProfiler rp;
  EquityModule em;
  GameState game(&rp, &em);

  Trainer trainer(&game);

  cout << "\n=== MCCFR Poker Engine ===\n";
  cout << "1. Train MCCFR\n";
  cout << "2. Play (MCCFR-Driven)\n";
  cout << "3. Exit\n";
  cout << "Select: ";

  int choice;
  cin >> choice;

  if (choice == 1) {
    train_mccfr(trainer);
    cout << "\n1. Train More\n";
    cout << "2. Play (MCCFR-Driven)\n";
    cout << "3. Exit\n";
    cout << "Select: ";
    cin >> choice;
  }

  if (choice == 2) {
    unified_play_mode(trainer);
  }

  return 0;
}