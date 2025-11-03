#ifndef ODDS_CALCULATOR_H
#define ODDS_CALCULATOR_H

#include <vector>
#include <string>
#include <array>

// Card representation
enum Rank { TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, JACK, QUEEN, KING, ACE };
enum Suit { CLUBS, DIAMONDS, HEARTS, SPADES };

struct Card {
    Rank rank;
    Suit suit;

    Card(Rank r, Suit s) : rank(r), suit(s) {}
};

// Hand strength for comparison
enum HandStrength {
    HIGH_CARD,
    PAIR,
    TWO_PAIR,
    THREE_OF_A_KIND,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    FOUR_OF_A_KIND,
    STRAIGHT_FLUSH,
    ROYAL_FLUSH
};

// Main odds calculator class
class OddsCalculator {
private:
    int monte_carlo_iterations;  // Number of simulations to run

    // Evaluate hand strength
    HandStrength evaluate_hand(const std::vector<Card>& hole_cards,
                               const std::vector<Card>& community_cards) const;

    // Run single Monte Carlo simulation
    bool simulate_hand_outcome(const std::vector<Card>& our_hole_cards,
                              const std::vector<Card>& community_cards,
                              int num_opponents) const;

public:
    OddsCalculator(int iterations = 10000);

    // Calculate baseline win probability for given hand
    // Returns probability 
    double calculate_win_probability(const std::vector<Card>& hole_cards,
                                     const std::vector<Card>& community_cards,
                                     int num_opponents) const;

    // Calculate odds for specific known hands
    double calculate_baseline_hand_odds(const std::string& hand_notation,
                                        int num_opponents) const;

    // Adjust odds based on risk profiler input
    double adjust_odds_for_context(double baseline_odds,
                                   double opponent_aggression,
                                   double opponent_tightness,
                                   double stack_depth_ratio) const;

    // Set number of Monte Carlo iterations
    void set_iterations(int iterations);
};

#endif
