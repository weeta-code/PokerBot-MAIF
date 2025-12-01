#include "node.h"


Node::Node(int n) {
    regret_sum = new double[n];
    strategy = new double[n];
    strategy_sum = new double[n];
    average_strategy = new double[n];
}

Node::~Node() {
    delete[] regret_sum;
    delete[] strategy;
    delete[] strategy_sum;
    delete[] average_strategy;
}

const double* Node::get_strategy(double realization_weight) {
    double normalizing_sum = 0;
    for (int a = 0; a < num_actions; a++) {
        strategy[a] = regret_sum[a] > 0 ? regret_sum[a] : 0;
        normalizing_sum += strategy[a];
    }

    for (int a = 0; a < num_actions; a++) {
        if (normalizing_sum > 0) strategy[a] /= normalizing_sum;
        else strategy[a] = 1.0 / num_actions;
        strategy_sum[a] += realization_weight * strategy[a];
    }

    return strategy;
};

const double* Node::get_average_strategy() {
    double* average_strategy = new double[num_actions];
    double normalizing_sum = 0;
    for (int a = 0; a < num_actions; a++) {
        normalizing_sum += strategy_sum[a];
    }
    for (int a = 0; a < num_actions; a++) {
        if (normalizing_sum > 0) average_strategy[a] = strategy_sum[a] / normalizing_sum;
        else average_strategy[a] = 1.0 / num_actions;
    }
    return average_strategy;
}

void Node::update_regret_sum(int action, double regret) {
    regret_sum[action] += regret;
}


