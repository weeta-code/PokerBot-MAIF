#ifndef RISK_PROFILER_H
#define RISK_PROFILER_H

#include <map>
#include <string>
#include <vector>

enum ProfileLabel {
  PROFILE_RISK_AVERSE,
  PROFILE_RISK_NEUTRAL,
  PROFILE_RISK_PRONE
};

struct PlayerProfile {
  int hands_observed;

  // Betting stats
  int hands_played;
  int hands_voluntarily_entered; // VPIP
  int hands_raised_preflop;      // PFR

  int total_bets;
  int total_calls;

  // Stack tracking
  double stack_size;
  double total_wagered;
  double current_bet_ratio; // wagered / initial_stack

  std::vector<double> bucket_histogram;

  ProfileLabel profile_label;
};

class RiskProfiler {
private:
  std::map<int, PlayerProfile> player_profiles;

  void update_profile_label(int player_id);

public:
  RiskProfiler(double risk_tolerance = 0.5);

  void add_player(int player_id, double initial_stack);

  void update_player_profile(int player_id, const std::string &action,
                             double bet_amount, double pot_size);

  void update_stack(int player_id, double amount);

  double calculate_risk_score(int player_id, const std::string &action,
                              double bet_amount, double pot_size,
                              double adjusted_win_probability) const;

  PlayerProfile get_player_profile(int player_id) const;

  void reset_hand();
};

#endif
