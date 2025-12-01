#include "train_state.h"

// fix this to be in game state
std::vector<int> raise_buckets = {1, 5, 10};

TrainState::TrainState(GameState *game) {
    game = game;
    game_over = false;
    next_player = 0;
    num_players = game->players.size();
    std::vector<bool> folded(num_players, false);
    
    for (int i = 0; i < num_players; ++i) {
        initial_stacks[i] = game->players[i].stack;
        
        // um
        player_stacks[i] += initial_stacks[i];
    }
}

TrainState::TrainState(const TrainState& other) {
    game = other.game;
    game_over = other.game_over;
    next_player = other.next_player;
    pot = other.pot;
    num_players = other.num_players;
    initial_stacks = other.initial_stacks;
    player_stacks = other.player_stacks;
    folded = other.folded;
}

TrainState& TrainState::operator=(const TrainState& other) {
    if (this != &other) {
        game = other.game;
        game_over = other.game_over;
        next_player = other.next_player;
        pot = other.pot;
        num_players = other.num_players;
        initial_stacks = other.initial_stacks;
        player_stacks = other.player_stacks;
        folded = other.folded;
    }
    return *this;
}

bool TrainState::is_terminal() {
    return game_over;
}

bool TrainState::is_chance() {
    // not decision not or end game
    return !game.stage && !game_over;
}

std::vector<Action> TrainState::get_legal_action() {
    if (is_chance()) {
        return { Action(ActionType::BET,0) };
    }

    if (game_over) {
        return {};
    }

    // check last action
    Action call = history_get_last_action();

    int tab = game.current_street_highest_bet;

    std::vector<Action> actions;
    if (call.type == ActionType::BET || call.type == ActionType::RAISE) {
        actions.emplace_back(ActionType::CHECK, 0);

        for (int s : raise_buckets) {
            if (s <= tab) actions.emplace_back(ActionType::BET, s);
        }

        actions.emplace_back(ActionType::ALLIN, tab);
    } else {
        actions.emplace_back(ActionType::FOLD, 0);
        int call_amt = std::min(tab, call.amount);
        actions.emplace_back(ActionType::CALL, call_amt);

        for (int s : raise_buckets) {
            if (s > call.amount && s <= tab) actions.emplace_back(ActionType::RAISE, s);
        }

        if (tab > call.amount) {
            actions.emplace_back(ActionType::ALLIN, tab);
        }
    }
    return actions;
}

void TrainState::apply_action(Action action) {
    if (is_chance()) {
        game.next_street();
    }
}