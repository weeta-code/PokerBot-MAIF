#ifndef NODE_H
#define NODE_H

class Node {
    private:
        int num_actions;
        double *regret_sum;
        double *strategy;
        double *strategy_sum;
        double *average_strategy;

    public:
        explicit Node(int n);

        ~Node() {};

        const double *get_strategy(double realization_weight);

        const double* get_average_strategy();

        void update_regret_sum(int action, double regret);

};


#endif