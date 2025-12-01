#ifndef NODE_H
#define NODE_H

#include <vector>

class Node {
private:
  int num_actions;
  std::vector<double> regret_sum;
  std::vector<double> strategy;
  std::vector<double> strategy_sum;

public:
  explicit Node(int n);

  ~Node() = default;

  std::vector<double> get_strategy(double realization_weight);
  std::vector<double> get_average_strategy();

  void update_regret_sum(int action, double regret);
};

#endif