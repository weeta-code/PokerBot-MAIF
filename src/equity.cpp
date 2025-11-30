#include "../include/equity.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>

// --- Internal Hand Evaluation Logic ---

// Helper to sort cards by rank descending
bool compareCards(const Card &a, const Card &b) { return a.rank > b.rank; }

int EquityModule::evaluate_5_cards(const std::vector<Card> &cards) {
  if (cards.size() != 5)
    return 0;

  std::vector<Card> sorted = cards;
  std::sort(sorted.begin(), sorted.end(), compareCards);

  bool flush = true;
  for (size_t i = 1; i < 5; ++i) {
    if (sorted[i].suit != sorted[0].suit) {
      flush = false;
      break;
    }
  }

  bool straight = true;
  for (size_t i = 0; i < 4; ++i) {
    if (sorted[i].rank != sorted[i + 1].rank + 1) {
      straight = false;
      break;
    }
  }
  // Wheel case: A 5 4 3 2
  if (!straight && sorted[0].rank == Rank::ACE &&
      sorted[1].rank == Rank::FIVE && sorted[2].rank == Rank::FOUR &&
      sorted[3].rank == Rank::THREE && sorted[4].rank == Rank::TWO) {
    straight = true;
  }

  if (flush && straight) {
    if (sorted[0].rank == Rank::ACE && sorted[1].rank == Rank::KING)
      return 0x900000;                // Royal
    return 0x800000 + sorted[0].rank; // Straight Flush
  }

  std::map<Rank, int> counts;
  for (const auto &c : sorted)
    counts[c.rank]++;

  bool four_kind = false;
  bool three_kind = false;
  int pairs = 0;

  for (auto const &[rank, count] : counts) {
    if (count == 4)
      four_kind = true;
    if (count == 3)
      three_kind = true;
    if (count == 2)
      pairs++;
  }

  if (four_kind)
    return 0x700000; // Simplified score
  if (three_kind && pairs >= 1)
    return 0x600000;
  if (flush)
    return 0x500000;
  if (straight)
    return 0x400000;
  if (three_kind)
    return 0x300000;
  if (pairs == 2)
    return 0x200000;
  if (pairs == 1)
    return 0x100000;

  return sorted[0].rank; 
}

int EquityModule::evaluate_7_cards(const std::vector<Card> &cards) {
  if (cards.size() < 5)
    return 0;
  if (cards.size() == 5)
    return evaluate_5_cards(cards);

  int max_score = 0;
  std::vector<int> p(cards.size());
  std::fill(p.begin(), p.begin() + 5, 1);
  std::fill(p.begin() + 5, p.end(), 0);

  do {
    std::vector<Card> combo;
    for (size_t i = 0; i < cards.size(); ++i) {
      if (p[i])
        combo.push_back(cards[i]);
    }
    int score = evaluate_5_cards(combo);
    if (score > max_score)
      max_score = score;
  } while (std::prev_permutation(p.begin(), p.end()));

  return max_score;
}

int EquityModule::bucketize_hand(const std::vector<Card> &hero_hand,
                                 const std::vector<Card> &board_cards,
                                 street /*street*/) {
  if (hero_hand.empty())
    return BucketID::AIR;

  std::vector<Card> all_cards = hero_hand;
  all_cards.insert(all_cards.end(), board_cards.begin(), board_cards.end());

  int score = evaluate_7_cards(all_cards);

  if (score >= 0x400000)
    return BucketID::STRONG_MADE;
  if (score >= 0x300000)
    return BucketID::STRONG_MADE;
  if (score >= 0x200000)
    return BucketID::TOP_PAIR;

  if (score >= 0x100000) {
    Rank board_high = Rank::TWO;
    for (const auto &c : board_cards)
      if (c.rank > board_high)
        board_high = c.rank;

    bool pocket_pair = (hero_hand[0].rank == hero_hand[1].rank);

    if (pocket_pair) {
      if (hero_hand[0].rank > board_high)
        return BucketID::OVER_PAIR;
      if (hero_hand[0].rank == board_high)
        return BucketID::TOP_PAIR;
      return BucketID::WEAK_PAIR;
    }

    for (const auto &c : hero_hand) {
      if (c.rank == board_high)
        return BucketID::TOP_PAIR;
    }
    return BucketID::MIDDLE_PAIR;
  }

  return BucketID::AIR;
}

double EquityModule::compute_equity_vs_range(
    int hero_bucket, const std::vector<double> &villain_bucket_distribution) {

  double total_equity = 0.0;

  for (size_t v_bucket = 0; v_bucket < villain_bucket_distribution.size();
       ++v_bucket) {
    double prob = villain_bucket_distribution[v_bucket];
    if (prob <= 0.0)
      continue;

    double win_rate = 0.5; 

    if (hero_bucket > (int)v_bucket) {
      win_rate = 1.0;
    } else if (hero_bucket < (int)v_bucket) {
      win_rate = 0.0;
    } else {
      win_rate = 0.5;
    }

    total_equity += prob * win_rate;
  }

  return total_equity;
}

double EquityModule::compute_baseline_value(double equity, double pot_size) {
  return equity * pot_size;
}

EquitySummary EquityModule::summarize_state(
    const std::vector<Card> &hero_hole_cards,
    const std::vector<Card> &board_cards,
    const std::vector<double> &villain_bucket_distribution, double pot_size,
    street street) {

  EquitySummary summary;
  summary.hero_bucket = bucketize_hand(hero_hole_cards, board_cards, street);
  summary.equity =
      compute_equity_vs_range(summary.hero_bucket, villain_bucket_distribution);
  summary.baseline = compute_baseline_value(summary.equity, pot_size);

  return summary;
}
