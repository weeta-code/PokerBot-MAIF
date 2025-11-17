#ifndef RISK_PROFILER_H
#define RISK_PROFILER_H

#include <vector>
#include <map>
#include <string>

struct PlayerProfile {
    double aggression_frequency;    
    double bluff_frequency;          
    double avg_bet_size_ratio;        
    int hands_observed;            
};

struct StackTracker {
    double initial_stack;
    double current_stack;
    double total_committed_this_hand;
    std::vector<double> bet_history;   

    double get_stack_percentage_committed() const;
};

class RiskProfiler {
private:
    std::map<std::string, PlayerProfile> player_profiles;
    std::map<std::string, StackTracker> stack_trackers;
    double risk_tolerance;             

public:
    RiskProfiler(double risk_tolerance = 0.5);

    void add_player(const std::string& player_id, double initial_stack);

    void update_player_profile(const std::string& player_id,
                               const std::string& action,
                               double bet_amount,
                               double pot_size);

    void update_stack(const std::string& player_id, double amount);

    double calculate_risk_score(const std::string& player_id,
                               const std::string& action,
                               double bet_amount,
                               double pot_size,
                               double adjusted_win_probability) const;

    PlayerProfile get_player_profile(const std::string& player_id) const;

    void reset_hand();
};

#endif 
