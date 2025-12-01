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

// defining a comprehensive player profile
struct Player {
  int id;
  vector<Card> hole_cards;
  int stack;
  int current_bet; // amount to bet in the current street
  bool is_folded;
  bool is_all_in;
  bool is_human;

  // useful stats for risk profiling
  int times_folded;
  int times_raised;
  int times_called;
  int hands_played;

  PlayerProfile profile;

  Player(int _id, double _stack, bool _is_human);
};

struct Action {
  int player_id;
  ActionType type;
  int amount;

  Action(int pid, ActionType t, int amt = 0)
      : player_id(pid), type(t), amount(amt) {}
};

struct GameState {
  vector<Player> players;
  vector<Card> deck;
  vector<Card> community_cards;

  Stage stage;
  std::vector<Action> history;
  int pot_size;
  int current_street_highest_bet;

  int num_players;
  int dealer_index;
  int current_player_index;

  int small_blind_pos;
  int big_blind_pos;
  int small_blind_amount;
  int big_blind_amount;

  // incapsulate other modules to external engines
  RiskProfiler *risk_profiler;
  EquityModule *equity_module;

  GameState(RiskProfiler *rp, EquityModule *em);

  // init funcs
  void init_game_setup();
  void init_deck();
  void shuffle_deck();

  // step by step in a street
  void start_hand();
  void deal_community_cards(); // for flop/turn/river
  void next_street();
  void resolve_winner();
  bool is_hand_over();
  bool is_betting_round_over();

  // action func
  Card draw_card();
  // returns true if action is valid
  bool record_action(int player_idx, Action action);

  // MCCFR methods
  std::vector<Action> get_legal_actions();
  void apply_action(Action action);
  bool is_terminal();
  string compute_information_set(int player_id);
  Player *get_player(int player_id);

  // helper funcs
  int get_active_player_count();
  Player *get_current_player();
  void next_player();

  // helper functions kat
  Action get_last_action();
};

#endif