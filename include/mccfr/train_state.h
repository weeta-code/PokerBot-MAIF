#ifndef TRAIN_STATE_H
#define TRAIN_STATE_H

#include "game_state.h"
#include <vector>
#include <set>

// this needs to be reconciled with game state
// but i don't even know where to start,,,

class TrainState {
    private:
        GameState game;
        bool game_over;
        int next_player;
        double pot; 
        int num_players;

        std::vector<double> initial_stacks;
        std::vector<double> player_stacks;
        std::vector<bool> folded;

    public:
        explicit TrainState(GameState *game);

        TrainState(const TrainState& other);

        TrainState& operator=(const TrainState& other);

        bool is_terminal();

        bool is_chance();

        std::vector<Action> get_legal_action();

        void apply_action(Action action);

        TrainState clone() const { return *this; }
};

#endif