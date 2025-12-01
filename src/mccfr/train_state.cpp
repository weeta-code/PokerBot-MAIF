#include "train_state.h"

// fix this to be in game state
std::vector<int> raise_buckets = {1, 5, 10};

TrainState::TrainState(GameState *game) {
    game = game;
    game_over = false;
    next_player = 0;
    num_players = game->players.size();
    pot = game->pot_size;
    folded = vector(num_players, false);
    all_in = vector(num_players, false);
    
    for (int i = 0; i < num_players; ++i) {
        initial_stacks[i] = game->players[i].stack;
        
        player_stacks[i] = initial_stacks[i];
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
        all_in = other.all_in;
    }
    return *this;
}

bool TrainState::is_terminal() {
    return game_over;
}

bool TrainState::is_chance() {
    return game.stage != Stage::START && !game_over;
}

std::vector<Action> TrainState::get_legal_actions() {
    Player* player = game.get_current_player();

    if (is_chance()) {
        return { Action(*player, ActionType::BET,0) };
    }

    if (game_over) {
        return {};
    }

    Action call = game.get_last_action();

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
        call_amount = std::min(tab, call.amount);
        actions.emplace_back(ActionType::CALL, call_amount);

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
        game.next_player();
        Player *next_player = game.get_current_player();
    }

    if (game_over) {
        return;
    }

    Player *player = game.get_current_player();
    if (!player) {
        return;
    }

    std::vector<Action> legal_actions = get_legal_actions();

    if (legal_actions.empty()) {
        return;
    }

    int player_id = (int) player->id;
    
    switch (action.type) {
        case ActionType::FOLD:
            player->is_folded = true;
            folded[player_id] = true;
            break;

        case ActionType::CHECK: {
            call_amount = 0;
            break;
        }

        case ActionType::CALL: {
            pot += call_amount;
            player_stacks[player_id] -= call_amount;
            break;
        }

        case ActionType::BET: {
            call_amount = action.amount;
            pot += call_amount;
            player_stacks[player_id] -= call_amount;
            break;
        }

        case ActionType::RAISE: {
            call_amount = action.amount;
            pot += call_amount;
            player_stacks[player_id] -= call_amount;
            break;
        }

            
        case ActionType::ALLIN: {
            double amount = player_stacks[player_id];
            pot += amount;
            player_stacks[player_id] = 0.0;
            all_in[player_id] = true;
            break;
        }
    }
    
    game.next_player();
    
    if (get_game_over()) {
        game.resolve_winner();
    }
}

bool TrainState::get_game_over() {
    int active_count = get_num_active_players();
    
    if (active_count <= 1) {
        game_over = true;
        return true;
    }
    
    for (int i = 0; i < num_players; ++i) {
        if (!folded[i] && !all_in[i]) {
            return false;  
        }
    }
    
    return true; 
}

int TrainState::get_num_active_players() {
    return std::count(folded.begin(), folded.end(), false);
}