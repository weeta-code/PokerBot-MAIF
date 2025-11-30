#include "../include/risk_profiler.h"
#include <iostream>
#include <numeric>

RiskProfiler::RiskProfiler(double /*risk_tolerance*/) {
  // Constructor
}

void RiskProfiler::add_player(const std::string &player_id,
                              double initial_stack) {
  PlayerProfile profile;
  profile.hands_observed = 0;
  profile.hands_played = 0;
  profile.hands_voluntarily_entered = 0;
  profile.hands_raised_preflop = 0;
  profile.total_bets = 0;
  profile.total_calls = 0;

  profile.stack_size = initial_stack;
  profile.total_wagered = 0;
  profile.current_bet_ratio = 0;

  profile.profile_label = ProfileLabel::PROFILE_RISK_NEUTRAL;

  player_profiles[player_id] = profile;
}

void RiskProfiler::update_player_profile(const std::string &player_id,
                                         const std::string &action,
                                         double bet_amount,
                                         double /*pot_size*/) {
  if (player_profiles.find(player_id) == player_profiles.end())
    return;

  PlayerProfile &profile = player_profiles[player_id];

  // Update stack
  if (bet_amount > 0) {
    profile.stack_size -= bet_amount;
    profile.total_wagered += bet_amount;
  }

  // Update stats
  if (action == "bet" || action == "raise") {
    profile.total_bets++;
    if (profile.hands_observed >
        0) { // Assuming preflop logic handled elsewhere or simplified
             // In real system, check street
    }
  } else if (action == "call") {
    profile.total_calls++;
  }

  update_profile_label(player_id);
}

void RiskProfiler::update_stack(const std::string &player_id, double amount) {
  if (player_profiles.find(player_id) != player_profiles.end()) {
    player_profiles[player_id].stack_size = amount;
  }
}

double
RiskProfiler::calculate_risk_score(const std::string &player_id,
                                   const std::string & /*action*/,
                                   double bet_amount, double /*pot_size*/,
                                   double /*adjusted_win_probability*/) const {

  if (player_profiles.find(player_id) == player_profiles.end())
    return 0.5;

  const PlayerProfile &profile = player_profiles.at(player_id);

  // Risk score based on % of stack wagered and aggression
  double stack_risk = 0.0;
  double initial = profile.stack_size + profile.total_wagered; // Approx
  if (initial > 0) {
    stack_risk = (bet_amount + profile.total_wagered) / initial;
  }

  // Adjust by profile
  double multiplier = 1.0;
  if (profile.profile_label == PROFILE_RISK_PRONE)
    multiplier = 1.2;
  if (profile.profile_label == PROFILE_RISK_AVERSE)
    multiplier = 0.8;

  double score = stack_risk * multiplier;
  if (score > 1.0)
    score = 1.0;

  return score;
}

PlayerProfile
RiskProfiler::get_player_profile(const std::string &player_id) const {
  if (player_profiles.count(player_id)) {
    return player_profiles.at(player_id);
  }
  return PlayerProfile();
}

void RiskProfiler::reset_hand() {
  for (auto &[id, profile] : player_profiles) {
    profile.total_wagered = 0;
    profile.current_bet_ratio = 0;
    profile.hands_played++;
  }
}

void RiskProfiler::update_profile_label(const std::string &player_id) {
  PlayerProfile &profile = player_profiles[player_id];

  double af = 0;
  if (profile.total_calls > 0) {
    af = (double)profile.total_bets / profile.total_calls;
  } else if (profile.total_bets > 0) {
    af = 10.0; // High aggression
  }

  if (af > 2.0)
    profile.profile_label = PROFILE_RISK_PRONE;
  else if (af < 1.0)
    profile.profile_label = PROFILE_RISK_AVERSE;
  else
    profile.profile_label = PROFILE_RISK_NEUTRAL;
}
