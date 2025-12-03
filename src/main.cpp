#include "../include/game_state.h"
#include "../include/mccfr/trainer.h"
#include <cmath>
#include <iomanip>
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

std::vector<Card> ask_user_for_cards(Stage stage) {
  std::vector<Card> cards;

  if (stage == Stage::START) {
    cout << "Your hole cards (e.g., Ah Kd): ";
    string c1, c2;
    cin >> c1 >> c2;
    cards.push_back(parse_card(c1));
    cards.push_back(parse_card(c2));
  } else if (stage == Stage::PREFLOP) {
    cout << "\nDealing FLOP...\nEnter flop cards (e.g., 3h 7d Ks): ";
    string f1, f2, f3;
    cin >> f1 >> f2 >> f3;
    cards.push_back(parse_card(f1));
    cards.push_back(parse_card(f2));
    cards.push_back(parse_card(f3));
  } else if (stage == Stage::FLOP) {
    cout << "\nDealing TURN...\nEnter turn card (e.g., Qc): ";
    string t;
    cin >> t;
    cards.push_back(parse_card(t));
  } else if (stage == Stage::TURN) {
    cout << "\nDealing RIVER...\nEnter river card (e.g., 2d): ";
    string r;
    cin >> r;
    cards.push_back(parse_card(r));
  }

  return cards;
}

void unified_play_mode(Trainer &trainer) {
  RiskProfiler rp;
  EquityModule em;
  GameState game(&rp, &em);

  // Setup
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
  game.small_blind_amount = sb_amount;
  game.big_blind_amount = bb_amount;
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
  game.small_blind_pos = (dealer_pos + 1) % num_players;
  game.big_blind_pos = (dealer_pos + 2) % num_players;

  // Start hand
  game.start_hand();

  // Main State Machine Loop
  while (!game.is_terminal()) {
    if (game.type == StateType::CHANCE) {
      // Ask user for cards
      std::vector<Card> cards = ask_user_for_cards(game.stage);
      game.apply_chance(cards);

      // Print board after dealing
      if (game.stage != Stage::START && game.stage != Stage::PREFLOP) {
        cout << "Board: ";
        for (auto &c : game.community_cards)
          cout << c << " ";
        cout << "\n";
      }
    } else if (game.type == StateType::PLAY) {
      Player *p = game.get_current_player();

      if (p->is_folded || p->is_all_in) {
        game.determine_next_state();
        continue;
      }

      // Display game status
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

      // MCCFR Recommendation
      std::vector<double> probs;
      trainer.get_action_recommendation(game, p->id, probs);
      std::vector<Action> legal = game.get_legal_actions();

      if (!probs.empty() && !legal.empty()) {
        cout << "\n*** MCCFR Recommendation ***\n";

        bool is_uniform = true;
        double expected = 1.0 / probs.size();
        for (double prob : probs) {
          if (std::abs(prob - expected) > 1e-4) {
            is_uniform = false;
            break;
          }
        }

        if (is_uniform) {
          cout << "Suggested: None (Unexplored State - Using Uniform Random)\n";
        } else {
          int max_idx = 0;
          for (size_t i = 1; i < probs.size(); ++i) {
            if (probs[i] > probs[max_idx])
              max_idx = i;
          }
          Action best = legal[max_idx];
          cout << "Suggested: ";
          switch (best.type) {
          case ActionType::FOLD:
            cout << "FOLD";
            break;
          case ActionType::CHECK:
            cout << "CHECK";
            break;
          case ActionType::CALL:
            cout << "CALL " << best.amount;
            break;
          case ActionType::BET:
            cout << "BET " << best.amount;
            break;
          case ActionType::RAISE:
            cout << "RAISE to " << best.amount;
            break;
          case ActionType::ALLIN:
            cout << "ALL-IN " << best.amount;
            break;
          }
          cout << " (prob: " << std::fixed << std::setprecision(2)
               << probs[max_idx] * 100 << "%)\n";
        }
      }

      // Get action
      Action selected(-1, ActionType::FOLD, 0);

      if (p->is_human) {
        int call_amt = game.current_street_highest_bet - p->current_bet;

        cout << "\nWhat did you do?\n";
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
          int amt;
          cout << (call_amt == 0 ? "Bet amount: " : "Raise to: ");
          cin >> amt;
          cin.ignore(10000, '\n');
          if (call_amt == 0) {
            selected = Action(p->id, ActionType::BET, amt);
          } else {
            selected = Action(p->id, ActionType::RAISE, amt);
          }
        } else if (action == 4) {
          selected = Action(p->id, ActionType::ALLIN, p->stack);
        }
      } else {
        // Bot action - use MCCFR
        if (!legal.empty() && !probs.empty()) {
          double r = (double)rand() / RAND_MAX;
          double cumulative = 0.0;
          int chosen_idx = 0;
          for (size_t i = 0; i < probs.size(); ++i) {
            cumulative += probs[i];
            if (r <= cumulative) {
              chosen_idx = i;
              break;
            }
          }
          selected = legal[chosen_idx];

          cout << "Player " << p->id << " did: ";
          switch (selected.type) {
          case ActionType::FOLD:
            cout << "FOLD\n";
            break;
          case ActionType::CHECK:
            cout << "CHECK\n";
            break;
          case ActionType::CALL:
            cout << "CALL " << selected.amount << "\n";
            break;
          case ActionType::BET:
            cout << "BET " << selected.amount << "\n";
            break;
          case ActionType::RAISE:
            cout << "RAISE to " << selected.amount << "\n";
            break;
          case ActionType::ALLIN:
            cout << "ALL-IN " << selected.amount << "\n";
            break;
          }
        }
      }

      game.apply_action(selected);
    }
  }

  // Showdown
  cout << "\n=== HAND COMPLETE ===\n";
  if (game.get_active_player_count() == 1) {
    for (auto &p : game.players) {
      if (!p.is_folded) {
        p.stack += game.pot_size;
        cout << "Player " << p.id << " wins pot of " << game.pot_size
             << " (others folded)\n";
        cout << "Final stack: " << p.stack << "\n";
        break;
      }
    }
  } else {
    game.resolve_winner();
  }
}

void train_mccfr(Trainer &trainer) {
  int iterations;
  cout << "Enter training iterations: ";
  cin >> iterations;
  trainer.train(iterations);
  cout << "Training complete.\n";
}

int main(int argc, char *argv[]) {
  RiskProfiler rp;
  EquityModule em;
  GameState game(&rp, &em);
  Trainer trainer(&game);

  if (argc == 3 && string(argv[1]) == "--train") {
    int iterations = atoi(argv[2]);
    cout << "Training " << iterations << " iterations...\n";
    trainer.train(iterations);
    trainer.save_to_file("poker_model.dat");
    cout << "Complete! Saved to poker_model.dat\n";
    return 0;
  }

  cout << "\n=== MCCFR Poker Engine ===\n";
  cout << "1. Train MCCFR\n";
  cout << "2. Play (MCCFR-Driven)\n";
  cout << "3. Save Training Data\n";
  cout << "4. Load Training Data\n";
  cout << "5. Exit\n";
  cout << "Select: ";

  int choice;
  cin >> choice;

  if (choice == 1) {
    train_mccfr(trainer);
    cout << "\n1. Train More\n";
    cout << "2. Play (MCCFR-Driven)\n";
    cout << "3. Save Training Data\n";
    cout << "4. Exit\n";
    cout << "Select: ";
    cin >> choice;
    if (choice == 3) {
      trainer.save_to_file("poker_model.dat");
      return 0;
    }
  }

  if (choice == 2) {
    unified_play_mode(trainer);
  }

  if (choice == 3) {
    trainer.save_to_file("poker_model.dat");
  }

  if (choice == 4) {
    trainer.load_from_file("poker_model.dat");
    cout << "\n1. Train More\n";
    cout << "2. Play (MCCFR-Driven)\n";
    cout << "3. Save Training Data\n";
    cout << "4. Exit\n";
    cout << "Select: ";
    cin >> choice;
    if (choice == 1) {
      train_mccfr(trainer);
    } else if (choice == 2) {
      unified_play_mode(trainer);
    } else if (choice == 3) {
      trainer.save_to_file("poker_model.dat");
    }
  }

  return 0;
}