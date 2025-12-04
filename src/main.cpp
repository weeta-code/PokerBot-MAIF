#include "../include/game_state.h"
#include "../include/mccfr/trainer.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

// --- Helper Functions ---

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

std::vector<Card> parse_cards(const string &line) {
  std::vector<Card> cards;
  std::stringstream ss(line);
  std::string item;
  while (ss >> item) {
    cards.push_back(parse_card(item));
  }
  return cards;
}

// --- Solver Mode ---

void solver_mode(Trainer &trainer) {
  RiskProfiler rp;
  EquityModule em;
  GameState game(&rp, &em);

  cout << "\n=== MCCFR Poker Solver (Manual Mode) ===\n";

  int num_players;
  double stack, sb, bb;
  cout << "Number of players: ";
  cin >> num_players;
  cout << "Stack size: ";
  cin >> stack;
  cout << "Small Blind: ";
  cin >> sb;
  cout << "Big Blind: ";
  cin >> bb;
  cin.ignore(10000, '\n');

  game.init_game_setup(num_players, stack, sb, bb);

  while (true) {
    cout << "\n--- New Hand ---\n";
    game.start_hand();

    // 1. Preflop Input
    cout << "Enter Hero Cards (e.g. Ah Kd): ";
    string line;
    getline(cin, line);
    std::vector<Card> hero_cards = parse_cards(line);
    game.set_player_cards(0, hero_cards);

    // Main Loop
    while (!game.is_terminal()) {
      // Display State
      cout << "\n----------------------------------------\n";
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

      if (!game.community_cards.empty()) {
        cout << "Board: ";
        for (auto &c : game.community_cards)
          cout << c << " ";
        cout << "\n";
      }

      // Display Equity & Stats
      if (!hero_cards.empty()) {
        double eq =
            em.calculate_display_equity(hero_cards, game.community_cards);
        cout << "Hero Equity (vs Random): " << std::fixed
             << std::setprecision(1) << eq * 100 << "%\n";
      }

      // Display Active Player
      Player *p = game.get_current_player();
      cout << "Action on Player " << p->id;
      if (p->is_human)
        cout << " (HERO)";
      else
        cout << " (Opponent)";

      double to_call = game.current_street_highest_bet - p->current_bet;
      cout << "\n  Stack: " << std::fixed << std::setprecision(2) << p->stack;
      cout << " | Current Bet: " << p->current_bet;
      cout << " | To Call: " << to_call << "\n";

      if (!p->is_human) {
        cout << "  " << rp.get_formatted_stats(p->id) << "\n";
      }

      // MCCFR Recommendation (for Hero)
      if (p->is_human) {
        std::vector<double> probs;
        Action best = trainer.get_action_recommendation(game, p->id, probs);
        std::vector<Action> legal = game.get_legal_actions();

        cout << "\n*** Solver Recommendation ***\n";
        if (!probs.empty()) {
  
          // int max_idx = 0;
          // for (size_t i = 1; i < probs.size(); ++i) {
          //   if (probs[i] > probs[max_idx])
          //     max_idx = i;
          // }
          // Action best = legal[max_idx];

          cout << "Best Action: ";
          switch (best.type) {
          case ActionType::FOLD:
            cout << "FOLD";
            break;
          case ActionType::CHECK:
            cout << "CHECK";
            break;
          case ActionType::CALL:
            cout << "CALL " << std::fixed << std::setprecision(2)
                 << best.amount;
            break;
          case ActionType::BET:
            cout << "BET " << std::fixed << std::setprecision(2) << best.amount;
            break;
          case ActionType::RAISE:
            cout << "RAISE to " << std::fixed << std::setprecision(2)
                 << best.amount;
            break;
          case ActionType::ALLIN:
            cout << "ALL-IN " << std::fixed << std::setprecision(2)
                 << best.amount;
            break;
          }
          // cout << " (" << std::fixed << std::setprecision(1)
          //      << probs[max_idx] * 100 << "%)\n";
        } else {
          cout << "No recommendation available (Unexplored state)\n";
        }
      }

      // Input Action
      cout << "\nActions: (f)old, (c)heck/call, (b) <amt> bet/raise, (a)llin\n";
      cout << "Enter Action: ";
      string action_str;
      getline(cin, action_str);

      Action selected(p->id, ActionType::FOLD, 0);
      if (action_str == "f") {
        selected = Action(p->id, ActionType::FOLD);
      } else if (action_str == "c") {
        int call_amt = game.current_street_highest_bet - p->current_bet;
        if (call_amt == 0)
          selected = Action(p->id, ActionType::CHECK);
        else
          selected = Action(p->id, ActionType::CALL, call_amt);
      } else if (action_str == "a") {
        selected = Action(p->id, ActionType::ALLIN, p->stack);
      } else if (action_str[0] == 'b') {
        double amt = stod(action_str.substr(2));
        if (game.current_street_highest_bet == 0)
          selected = Action(p->id, ActionType::BET, amt);
        else
          selected = Action(p->id, ActionType::RAISE, amt);
      }

      game.apply_action(selected);

      // Check for Street Transition
      if (game.is_betting_round_over()) {
        if (game.stage == Stage::RIVER) {
          cout << "Hand Over (Showdown)\n";
          break;
        } else {
          // Advance Street & Ask for Cards
          game.next_street();
          cout << "\nDealing "
               << (game.stage == Stage::FLOP
                       ? "FLOP"
                       : (game.stage == Stage::TURN ? "TURN" : "RIVER"))
               << "...\n";
          cout << "Enter Board Cards: ";
          getline(cin, line);
          std::vector<Card> new_cards = parse_cards(line);
          std::vector<Card> current_board = game.community_cards;
          current_board.insert(current_board.end(), new_cards.begin(),
                               new_cards.end());
          game.set_community_cards(current_board);
        }
      }
    }

    cout << "Play another hand? (y/n): ";
    string ans;
    getline(cin, ans);
    if (ans != "y")
      break;
  }
}

// --- Main ---

int main(int argc, char *argv[]) {
  // Dummy GameState for Trainer init (will be replaced in solver_mode)
  RiskProfiler rp;
  EquityModule em;
  GameState game(&rp, &em);
  Trainer trainer(&game);

  if (argc == 3 && string(argv[1]) == "--train") {
    int iterations = atoi(argv[2]);
    cout << "Training " << iterations << " iterations...\n";
    trainer.train(iterations);
    trainer.save_to_file("poker_model.dat");
    return 0;
  }

  cout << "1. Train MCCFR\n";
  cout << "2. Solver Mode (Manual Input)\n";
  cout << "Select: ";
  int choice;
  cin >> choice;
  cin.ignore(10000, '\n');

  if (choice == 1) {
    int iter;
    cout << "Iterations: ";
    cin >> iter;
    trainer.train(iter);
    trainer.save_to_file("poker_model.dat");
  } else {
    // Try load model
    trainer.load_from_file("poker_model.dat");
    solver_mode(trainer);
  }

  return 0;
}