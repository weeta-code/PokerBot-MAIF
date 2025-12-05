#ifndef GAME_STATE_MODULE_H
#define GAME_STATE_MODULE_H

#include "equity.h"
#include "risk_profiler.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace std;

enum class Stage {
  START,
  PREFLOP,
  FLOP,
  TURN,
  RIVER,
  SHOWDOWN,
};

enum class ActionType { FOLD, CHECK, CALL, BET, RAISE, ALLIN };

// Minimal player struct for MCCFR
struct Player {
  int id;
  vector<Card> hole_cards;
  double stack;
  double current_bet; // amount to bet in the current street
  bool is_folded;
  bool is_all_in;
  bool is_human;

  bool has_acted_this_street;

  Player(int _id, double _stack, bool _is_human);
};

struct Action {
  int player_id;
  ActionType type;
  double amount;

  Action(int pid, ActionType t, double amt = 0)
      : player_id(pid), type(t), amount(amt) {}
};

enum class StateType { CHANCE, PLAY, TERMINAL };

struct GameState {
  vector<Player> players;
  vector<Card> community_cards; // Manual input

  std::vector<Action> history;

  // Modules
  RiskProfiler *risk_profiler;
  EquityModule *equity_module;

  double pot_size;
  double current_street_highest_bet;

  int num_players;
  int dealer_index;
  int current_player_index;

  double small_blind_amount;
  double big_blind_amount;

  Stage stage;
  StateType type;

  GameState(RiskProfiler *rp, EquityModule *em);

  // Init
  void init_game_setup(int n_players, double stack_size, double sb, double bb);
  void start_hand();

  // Manual Input Methods
  void set_community_cards(const std::vector<Card> &cards);
  void set_player_cards(int player_id, const std::vector<Card> &cards);

  // Flow
  void next_street();
  void resolve_winner(); // Manual winner resolution or simple equity calc
  bool is_hand_over();
  bool is_betting_round_over();

  // Actions
  bool record_action(int player_idx, Action action);
  std::vector<Action> get_legal_actions();
  void apply_action(Action action);

  // MCCFR Support
  bool is_terminal();
  string compute_information_set(int player_id);
  Player *get_player(int player_id);

  // Abstraction Helpers
  int abstract_stack_size(double stack_bb) const;
  int abstract_pot_size(double pot_bb) const;
  std::string abstract_bet_size(double bet_amount) const;
  std::string abstract_action_history() const;

  // Helpers
  int get_active_player_count();
  Player *get_current_player();
  void next_player();
  void determine_next_state();
};

#endif