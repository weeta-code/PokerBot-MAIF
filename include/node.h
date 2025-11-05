#ifndef NODE_H
#define NODE_H

#include <string>
#include <iostream>
#include <vector>


class Node {
private:
    int _NUM_ACTIONS;
    std::string _info_set;
    double* regret_sum;
    double* strategy;
    double* strategy_sum;

public:
    Node(int n) {
        _NUM_ACTIONS = n;
        regret_sum = new double[n];
        strategy = new double[n];
        strategy_sum = new double[n];
    };
    ~Node() {};

    double* get_strategy(double realization_weight) {
        double normalizing_sum = 0;
        for (int a = 0; a < _NUM_ACTIONS; a++) {
            strategy[a] = regret_sum[a] > 0 ? regret_sum[a] : 0;
            normalizing_sum += strategy[a];
        }

        for (int a = 0; a < _NUM_ACTIONS; a++) {
            if (normalizing_sum > 0) strategy[a] /= normalizing_sum;
            else strategy[a] = 1.0 / _NUM_ACTIONS;
            strategy_sum[a] += realization_weight * strategy[a];
        }

        return strategy;
    };

    double* get_average_strategy() {
        double* average_strategy = new double[_NUM_ACTIONS];
        double normalizing_sum = 0;
        for (int a = 0; a < _NUM_ACTIONS; a++) {
            normalizing_sum += strategy_sum[a];
        }
        for (int a = 0; a < _NUM_ACTIONS; a++) {
            if (normalizing_sum > 0) average_strategy[a] = strategy_sum[a] / normalizing_sum;
            else average_strategy[a] = 1.0 / _NUM_ACTIONS;
        }
        return average_strategy;
    }

    // TODO: add more functions... tbd
};


#endif




