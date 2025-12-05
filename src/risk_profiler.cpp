#include "../include/risk_profiler.h"
#include <iomanip>
#include <iostream>
#include <sstream>

RiskProfiler::RiskProfiler() {
  // Constructor
}

void RiskProfiler::add_player(int player_id, double initial_stack) {
  PlayerProfile profile;
  profile.hands_played = 0;
  profile.hands_voluntarily_entered = 0;
  profile.hands_raised_preflop = 0;
  profile.total_bets = 0;
  profile.total_calls = 0;
  profile.stack_size = initial_stack;

  player_profiles[player_id] = profile;
}

void RiskProfiler::update_player_profile(int player_id,
                                         const std::string &action,
                                         double bet_amount,
                                         double /*pot_size*/) {
  if (player_profiles.find(player_id) == player_profiles.end())
    return;

  PlayerProfile &profile = player_profiles[player_id];

  // Update stack
  if (bet_amount > 0) {
    profile.stack_size -= bet_amount;
  }

  // Update stats
  if (action == "bet" || action == "raise" || action == "allin") {
    profile.total_bets++;
    // Note: In a real system we'd check street for PFR/VPIP more carefully
    // For now, we rely on the caller to update hands_voluntarily_entered etc.
  } else if (action == "call") {
    profile.total_calls++;
  }
}

void RiskProfiler::update_stack(int player_id, double amount) {
  if (player_profiles.find(player_id) != player_profiles.end()) {
    player_profiles[player_id].stack_size = amount;
  }
}

PlayerProfile RiskProfiler::get_player_profile(int player_id) const {
  if (player_profiles.count(player_id)) {
    return player_profiles.at(player_id);
  }
  return PlayerProfile();
}

void RiskProfiler::reset_hand() {
  for (auto &[id, profile] : player_profiles) {
    profile.hands_played++;
  }
}

std::string RiskProfiler::get_formatted_stats(int player_id) const {
  if (player_profiles.find(player_id) == player_profiles.end())
    return "N/A";

  const auto &p = player_profiles.at(player_id);

  double vpip = 0.0;
  double pfr = 0.0;
  double af = 0.0;

  if (p.hands_played > 0) {
    vpip = (double)p.hands_voluntarily_entered / p.hands_played * 100.0;
    pfr = (double)p.hands_raised_preflop / p.hands_played * 100.0;
  }

  if (p.total_calls > 0) {
    af = (double)p.total_bets / p.total_calls;
  } else if (p.total_bets > 0) {
    af = 10.0; // Cap at 10
  }

  std::stringstream ss;
  ss << std::fixed << std::setprecision(1);
  ss << "VPIP: " << vpip << "% | PFR: " << pfr << "% | AF: " << af;
  return ss.str();
}
