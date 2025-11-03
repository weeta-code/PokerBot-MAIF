#ifndef RISK_PROFILER_H
#define RISK_PROFILER_H

#include <vector>
#include <map>
#include <string>

// Player profile to track betting tendencies
struct PlayerProfile {
    double aggression_frequency;      // How often player bets/raises
    double bluff_frequency;            // Estimated bluff rate
    double avg_bet_size_ratio;         // Average bet size relative to pot
    int hands_observed;                // Sample size for profile accuracy
};

// Tracks stack and betting history
struct StackTracker {
    double initial_stack;
    double current_stack;
    double total_committed_this_hand;
    std::vector<double> bet_history;   // Track bets per round

    double get_stack_percentage_committed() const;
};

// Main risk profiler class
class RiskProfiler {
private:
    std::map<std::string, PlayerProfile> player_profiles;
    std::map<std::string, StackTracker> stack_trackers;
    double risk_tolerance;             // User-defined risk acceptance level (0.0 - 1.0)

public:
    RiskProfiler(double risk_tolerance = 0.5);

    // Initialize tracking for a new player
    void add_player(const std::string& player_id, double initial_stack);

    // Update player profile based on observed action
    void update_player_profile(const std::string& player_id,
                               const std::string& action,
                               double bet_amount,
                               double pot_size);

    // Update stack tracker when player commits chips
    void update_stack(const std::string& player_id, double amount);

    // Calculate risk score for a given action (0.0 = low risk, 1.0 = high risk)
    double calculate_risk_score(const std::string& player_id,
                               const std::string& action,
                               double bet_amount,
                               double pot_size,
                               double adjusted_win_probability) const;

    // Get player profile
    PlayerProfile get_player_profile(const std::string& player_id) const;

    // Reset for new hand
    void reset_hand();
};

#endif // RISK_PROFILER_H
