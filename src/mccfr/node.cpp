#include "../include/mccfr/node.h"
#include <numeric>

Node::Node(int n) : num_actions(n) {
  regret_sum.resize(n, 0.0);
  strategy.resize(n, 0.0);
  strategy_sum.resize(n, 0.0);
}

std::vector<double> Node::get_strategy(double realization_weight) {
  double normalizing_sum = 0;
  for (int a = 0; a < num_actions; a++) {
    strategy[a] = regret_sum[a] > 0 ? regret_sum[a] : 0;
    normalizing_sum += strategy[a];
  }

  for (int a = 0; a < num_actions; a++) {
    if (normalizing_sum > 0)
      strategy[a] /= normalizing_sum;
    else
      strategy[a] = 1.0 / num_actions;
    strategy_sum[a] += realization_weight * strategy[a];
  }

  return strategy;
}

std::vector<double> Node::get_average_strategy() {
  std::vector<double> avg_strategy(num_actions);
  double normalizing_sum = 0;
  for (int a = 0; a < num_actions; a++) {
    normalizing_sum += strategy_sum[a];
  }
  for (int a = 0; a < num_actions; a++) {
    if (normalizing_sum > 0)
      avg_strategy[a] = strategy_sum[a] / normalizing_sum;
    else
      avg_strategy[a] = 1.0 / num_actions;
  }
  return avg_strategy;
}

void Node::update_regret_sum(int action, double regret) {
  regret_sum[action] += regret;
}
