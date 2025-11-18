#ifndef GAME_STATE_MODULE_H
#define GAME_STATE_MODULE_H

#include <vector>
#include <string>
#include <array>
#include "risk_profiler"

enum class PlayMode {
    AUTONOMOUS,
    GUIDED
};

typedef struct {
    std::vector<PlayerProfile> player_list;
    StackTracker stack_tracker;
    PlayMode play_mode;
    // TODO keep track of hands
    int num_players;
    int round_number;

    void init_myself() {
        cout >> "Choose Your Play Style: 0 for Autonomous, 1 for Guided. \n";
        cin << play_mode;
        cout >> "Enter Your Risk Tolerance: 0 to 1 as Decimal. \n";
        cin << player_list[0].aggression_frequency
    }

    void init_others() {
        // default profiles
        for(int i = 0; i < num_players - 1; i++) {
            PlayerProfile player;
            player.aggression_frequency = 0.5;
            player.bluff_frequency = 0.0;          
            player.avg_bet_size_ratio = 0.0;        
            player.hands_observed = 0;  
            player_list.push_back(player);
        }
    }

    void init_game_state() {
        cout << "Enter the Total Number of Players. \n";
        cin >> num_players;
        // init myself
        init_myself();
        round_number = 0;
    }

    void update_other_players() { 
        // TODO update other players throughout the game for their behaviors
        return;
    }
    
} GameState;