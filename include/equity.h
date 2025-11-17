#ifndef EQUITY_MODULE_H
#define EQUITY_MODULE_H

#include <vector>
#include <string>
#include <array>

enum Rank { TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, JACK, QUEEN, KING, ACE };
enum Suit { CLUBS, DIAMONDS, HEARTS, SPADES };

struct Card {
    Rank rank;
    Suit suit;

    Card(Rank r, Suit s) : rank(r), suit(s) {}
};

enum BucketID {
  AIR = 0,
  WEAK_BACKDOOR = 1,
  WEAK_DRAW = 2,
  STRONG_DRAW = 3,
  WEAK_PAIR = 4,
  MIDDLE_PAIR = 5,
  TOP_PAIR = 6,
  OVER_PAIR = 7,
  STRONG_MADE = 8,
  NUTS = 9
};

struct EquitySummary {
  int hero_bucket;
  double equity;
  double baseline;
};

enum street {
  PRE,
  FLOP,
  TURN,
  RIVER
};

class EquityModule {
private:
  int monte_carlo_iterations;  

  int bucketize_hand(std::vector<Card> hero_hand, std::vector<Card> board_cards, std::string street);

  double compute_equity_vs_range(std::vector<Card> hero_hand, std::vector<Card> board_cards, std::vector<double> villain_bucket_distribution);

  std::pair<Card, Card> sample_villain_hand_from_bucket(BucketID);

  double compute_baseline_value(double equity, double pot_size);

  double evaluate_showdown(std::vector<Card> hero_hand, std::vector<Card> villain_hand, std::vector<Card> full_board);

  std::vector<std::vector<Card>> generate_remaining_boards(std::vector<Card> board_cards);
public:
  EquityModule(int iterations = 10000);
  void set_iterations(int iterations);

  EquitySummary summarize_state(std::vector<Card> hero_hole_cards, std::vector<Card> board_cards, 
                                std::vector<double> villain_bucket_distribution, double pot_size, street street);
};

#endif
