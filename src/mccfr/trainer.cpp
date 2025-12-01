#include "trainer.h"
#include "node.h"

Trainer::Trainer(GameState *game) {
    game = game;
    fixed_strategies = new std::unordered_map<InfoSets, Node *>[game->num_players];
}

Trainer::~Trainer() {
    for (auto &itr : node_map) {
        delete itr.second;
    }
    for (int i = 0; i < game.num_players; ++i) {
        if (update[i]) {
            continue;
        }
        for (auto &itr : fixed_strategies[i]) {
            delete itr.second;
        }
    }
    delete[] fixed_strategies;
    delete[] update;
    delete &game;
}

void Trainer::train(int iterations) {
    std::vector<double> utils(game.get_legal_action().size(), 0.0);

    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < game.num_players; ++p) {
            if (update[p]) {
                continue;
            }
           
            utils[p] = cfr(game, p, std::vector<double>(game.num_players, 1));
            for (auto & itr : node_map) {
                itr.second->update_regret_sum();
            }
        }
    }
}

const double *Trainer::get_strategy(InfoSets info_set) {
    auto it = node_map.find(info_set);
    if (it != node_map.end()) {
        return it->second->get_average_strategy();
    }
    return {};
}

std::unordered_map<InfoSets, const double*>  Trainer::get_overall_strategy() {
    std::unordered_map<InfoSets, const double*> strategies;
    
    for (const auto& [info_set, node] : node_map) {
        strategies[info_set] = node->get_average_strategy();
    }
    
    return strategies;
}

double Trainer::cfr(GameState state, int player, std::vector<double> p) {
    if (state.is_terminal()) {
        auto utils = state.get_returns(); // get payoff at state
        return utils[player];
    }
    
    if (state.is_chance()) {
        state.next_street(); // deal next street
        return cfr(clone(state), player, p);
    }
    
    int curr_player = state.get_current_player();
    InfoSets info_set = state.get_info_set(curr_player);
    
    auto legal_actions = state->get_legal_action();
    if (legal_actions.empty()) {
        return;
    }
    
    if (node_map.find(info_set) == node_map.end()) {
        node_map.emplace(info_set, Node(legal_actions.size()));
    }
    
    Node *node = node_map[info_set];
    
    if (curr_player == player) {
        auto strategy = node->get_strategy(p[player]);
        std::vector<double> utils(legal_actions.size(), 0.0);
        double node_util = 0.0;
        
        for (std::size_t i = 0; i < legal_actions.size(); ++i) {
            auto nextState = get_next_state(); //working on logic
            nextState->apply_action(legal_actions[i]);
            
            std::vector<double> next_p = p;
            next_p[player] *= strategy[i];
            
            utils[i] = cfr(clone(nextState), player, next_p);
            node_util += strategy[i] * utils[i];
        }
        
        for (std::size_t i = 0; i < legal_actions.size(); ++i) {
            double regret = utils[i] - node_util;
            node->update_regret_sum(i, regret);
        }
        
        return node_util;
        
    } else {
        auto strategy = node->get_strategy(p[curr_player]);
        int action = sample_action(strategy); // sampler?
        
        std::vector<double> next_p = p;
        next_p[curr_player] *= strategy[action];
        
        state->apply_action(legal_actions[action]);
        return cfr(clone(state), player, next_p);
    }
}
