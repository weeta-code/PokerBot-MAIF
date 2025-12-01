#ifndef TRAINER_H
#define TRAINER_H

#include <vector>
#include <unordered_map>
#include "game_state.h"
#include "node.h"
#include "train_state.h"

using InfoSets = typename std::unordered_map<std::string, std::vector<std::tuple<std::string, double>>>;

class Trainer {
    private:
        GameState game;
        TrainState train_state;
        std::unordered_map<InfoSets, Node *> node_map;
        std::unordered_map<InfoSets, Node *> *fixed_strategies;
        uint64_t count;
        bool *update;

        double cfr(TrainState state, int player, std::vector<double> p);

    public:
        explicit Trainer(GameState *game);

        ~Trainer();

        void train(int iterations);

        const double *get_strategy(InfoSets info_set);

        std::unordered_map<InfoSets, const double*> get_overall_strategy();

};

#endif