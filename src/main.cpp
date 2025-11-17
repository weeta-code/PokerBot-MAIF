#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <cstdlib>
#include "../include/odds_calculator.h"
#include "../include/risk_profiler.h"

using namespace std;

enum class PlayMode {
    AUTONOMOUS,
    GUIDED
};

typedef struct {
    double riskTolerance;  // 1 means we can go all in, 0 means we don't want to lose anything
    string playerID; // player unique identifier
    PlayMode mode;
    PlayerProfile betStyle; //TODO maybe change the PlayerProfile name to something that can differentiate between PlayerConfig
} PlayerConfig;

typedef struct {
    std::string hand[2]; // hole cards 
    // int stackSize;   // player current chips
    int potSize;     // total pot
    int roundNumber;    // preflop, flop, turn, river as 0, 1, 2, 3
} GameState; // TODO figure out how to track player hands, via gamestate or playerconfig


PlayerConfig configurePlayer() {
    PlayerConfig playerCFG;
    playerCFG.playerID = std::to_string(rand());

    cout << "Starting Poker Engine\n";
    cout << "Enter Risk Tolerance ∈ (0.0, 1.0)\n";
    cin >> playerCFG.riskTolerance;

    int modeChoice;
    cout << "Enter Engine Mode:\n  0. Autonomous\n  1. Guided\n";
    cin >> modeChoice;

    (modeChoice == 0) ? playerCFG.mode = PlayMode::AUTONOMOUS : playerCFG.mode = PlayMode::GUIDED;

    return playerCFG;
}

GameState initGameState() {
    GameState state;
    cout << "Enter your hand (with space in between): \n";
    cin >> state.hand[0] >> state.hand[1];
    cout << "Enter current pot size: \n";
    cin >> state.potSize;
    state.roundNumber = 0; // Start at preflop
    return state;
}


void playRound(OddsCalculator& oddsCalc, RiskProfiler& riskProf, 
               const PlayerConfig& playerCFG, GameState& state) {
    // get win odds 
    // TODO implement string input parsing to read player input hand
    // TODO implement Monte Carlo and use them later
    double winProb = oddsCalc.calculate_baseline_hand_odds("A", 1);
    

    // update risk score based on game state
    // with placeholder values
    double riskScore = riskProf.calculate_risk_score(
        playerCFG.playerID, "call", state.potSize * 0.5, state.potSize, winProb
    );

    // combine odds and risk to make recommendation
    // TODO implement the methodology? weighted? wtv
    // placeholder
    double decisionScore = winProb - riskScore;

    cout << "Info Print";
    cout << "Hand: " << state.hand << endl;
    cout << "Win Probability: " << winProb * 100 << "%\n";
    cout << "Risk Score: " << riskScore * 100 << "%\n";
    cout << "Decision Score: " << decisionScore << "%\n";

    // TODO refine bot action logic and decision threshold
    if (playerCFG.mode == PlayMode::AUTONOMOUS) {
        if (decisionScore > 0.2)
            cout << "BOT Says Raise.\n";
        else if (decisionScore > 0.1)
            cout << "BOT Says Call.\n";
        else
            cout << "BOT Says Fold.\n";
    } else {
        cout << "SUGGESTED MOVE: ";
        if (decisionScore > 0.3) cout << "Raise\n";
        else if (decisionScore > 0.1) cout << "Call\n";
        else cout << "Fold\n";
    }

    // TODO implement state changes
    // placeholder 
    riskProf.update_stack(playerCFG.playerID, 10 - state.potSize * 0.5);
    // state.stackSize *= (1.0 - riskScore * 0.1);
    state.potSize *= 1.1;
    state.roundNumber++;

    cout << "State Print";
    // cout << "Next State Size: " << state.stackSize << "%\n";
    cout << "Next Pot Size: " << state.potSize << "\n";
    cout << "Next Round #: " << state.roundNumber << "\n";
}

// TODO maybe put state logic in another file? discuss later
int main() {
    // init
    PlayerConfig config = configurePlayer();
    GameState state = initGameState();
    // is stack tracker per player or for the whole game state
    // StackTracker tracker = 

    OddsCalculator oddsCalculator;
    RiskProfiler riskProfiler;

    cout << "Hello World!";

    // simulate some stuff
    for (int round = 0; round < 4; ++round) {
        playRound(oddsCalculator, riskProfiler, config, state);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    cout << "Goodbye!" << "%\n";
    return 0;
}
