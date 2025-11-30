#include "../include/equity.h"
#include "../include/game_state.h"
#include "../include/risk_profiler.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace std;

// bot logic
string get_bot_decision(GameState &state, Player *bot) {
  // use equity_module to evaluate hands and calculate equity
  // We need to construct villain distribution. For now, uniform.
  vector<double> villain_dist(10, 0.1);

  EquitySummary eq = state.equity_module->summarize_state(
      bot->hole_cards, state.community_cards, villain_dist, state.pot_size,
      (street)state.stage // Cast Stage to street enum if they match or map them
  );

  double win_prob = eq.equity;

  // get the bot's risk profiler
  // analyze if the human player is bluffing
  double risk_score = state.risk_profiler->calculate_risk_score(
      "MYSELF", // TODO: this should probably be the opponent's ID we are
                // profiling? Or self risk? The original code used "MYSELF" for
                // risk score in main.cpp, but here it's inside bot decision. If
                // this is the bot deciding, it should check risk of the
                // opponent (Human).
      "check", // TODO: get human player's last action
      0, state.pot_size, win_prob);

  // TODO: Monte Carlo process insert here
  // place holder: poker expected value formula -> EV = (%W * $W) – (%L * $L)
  double expected_value =
      win_prob * (state.pot_size + state.current_street_highest_bet) -
      (1.0 - win_prob) * (state.current_street_highest_bet - bot->current_bet);

  // placeholder thresholding based on expected_value and risk
  if (expected_value > 50 && win_prob > 0.6) {
    return "raise";
  } else if (win_prob > 0.3 ||
             (state.current_street_highest_bet == bot->current_bet)) {
    return "call";
  } else {
    return "fold";
  }
}

int main() {
  // init modules
  EquityModule equity_module;
  RiskProfiler risk_prof;

  // GameState takes pointers to tools it needs
  GameState state(&risk_prof, &equity_module);

  // init up game state
  state.init_game_setup();

  // core game loop
  bool game_running = true;
  while (game_running) {
    state.start_hand();

    while (state.stage != Stage::SHOWDOWN &&
           state.get_active_player_count() > 1) {

      // betting round
      // simplified logic
      // each player acts once per street or until bets match

      int players_to_act = state.get_active_player_count();
      int acts_remaining = players_to_act;

      while (acts_remaining > 0) {
        Player *current = state.get_current_player();

        if (current->is_folded || current->is_all_in) {
          state.next_player();
          continue;
        }

        cout << "\n> Turn: " << current->id << " (Pot: " << state.pot_size
             << ") \n";

        string action;
        double amount = 0;

        if (current->is_human) {
          // player input things
          if (state.play_mode == PlayMode::GUIDED) {
            // GUIDED MODE: could print engine advice here
            vector<double> villain_dist(10, 0.1);
            EquitySummary eq = equity_module.summarize_state(
                current->hole_cards, state.community_cards, villain_dist,
                state.pot_size, (street)state.stage);
            cout << "[ENGINE] Win Probability: " << (eq.equity * 100) << "%\n";
          }

          cout << "Action (call, raise, fold): ";
          cin >> action;
          if (action == "raise") {
            cout << "Amount (total bet): ";
            cin >> amount;
          }
        } else {
          // bot turn, bot logic
          // sleep so everything doesn't print to the console all at once
          std::this_thread::sleep_for(std::chrono::milliseconds(300));
          action = get_bot_decision(state, current);

          if (action == "raise") {
            // TODO: refine logic to calculate the bet amount for the bot, or
            // maybe this is good
            amount = state.current_street_highest_bet + (state.pot_size * 0.5);
          }
        }

        // execute and record actions
        bool valid =
            state.record_action(state.current_player_index, action, amount);
        if (!valid) {
          // forces fold if input action is not valid
          cout << "Invalid Move \n";
          state.record_action(state.current_player_index, "fold", 0);
        }

        state.next_player();
        acts_remaining--;
      }

      // finish turn and move to next street
      state.next_street();
    }

    state.resolve_winner();

    cout << "Play another hand? (1=yes, 0=no): ";
    int cont;
    cin >> cont;
    if (cont == 0)
      game_running = false;
  }

  return 0;
}