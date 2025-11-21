#include <cmath>

class OptionPricer {
public:
    static double normal_cdf(double x);

    static double black_scholes_call(double S, double K, double r, double q,
                                     double sigma, double T);
};


