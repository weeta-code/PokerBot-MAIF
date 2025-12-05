#ifndef RISK_PROFILER_H
#define RISK_PROFILER_H

#include <map>
#include <string>
#include <vector>

struct PlayerProfile {
  // Betting stats
  int hands_played;
  int hands_voluntarily_entered; // VPIP
  int hands_raised_preflop;      // PFR

  int total_bets;
  int total_calls;

  // Stack tracking
  double stack_size;
};

class RiskProfiler {
private:
  std::map<int, PlayerProfile> player_profiles;

public:
  RiskProfiler();

  void add_player(int player_id, double initial_stack);

  void update_player_profile(int player_id, const std::string &action,
                             double bet_amount, double pot_size);

  void update_stack(int player_id, double amount);

  PlayerProfile get_player_profile(int player_id) const;

  std::string get_formatted_stats(int player_id) const;

  void reset_hand();
};

#endif
