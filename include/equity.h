#ifndef EQUITY_MODULE_H
#define EQUITY_MODULE_H

#include <array>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

enum Rank {
  TWO,
  THREE,
  FOUR,
  FIVE,
  SIX,
  SEVEN,
  EIGHT,
  NINE,
  TEN,
  JACK,
  QUEEN,
  KING,
  ACE
};
enum Suit { CLUBS, DIAMONDS, HEARTS, SPADES };

struct Card {
  Rank rank;
  Suit suit;

  Card(Rank r, Suit s) : rank(r), suit(s) {}

  bool operator==(const Card &other) const {
    return rank == other.rank && suit == other.suit;
  }

  friend std::ostream &operator<<(std::ostream &os, const Card &c) {
    const char *ranks = "23456789TJQKA";
    const char *suits = "cdhs";
    os << ranks[c.rank] << suits[c.suit];
    return os;
  }
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

enum street { PRE, FLOP, TURN, RIVER };

class EquityModule {
public:
  EquitySummary
  summarize_state(const std::vector<Card> &hero_hole_cards,
                  const std::vector<Card> &board_cards,
                  const std::vector<double> &villain_bucket_distribution,
                  double pot_size, street street);

  void enable_cache();
  void clear_cache();

  int bucketize_hand(const std::vector<Card> &hero_hand,
                     const std::vector<Card> &board_cards, street street);

  int evaluate_7_cards(const std::vector<Card> &cards);

private:

  double compute_equity_vs_range(
      int hero_bucket, const std::vector<double> &villain_bucket_distribution);

  double compute_baseline_value(double equity, double pot_size);

  int evaluate_5_cards(const std::vector<Card> &cards);
};

#endif
