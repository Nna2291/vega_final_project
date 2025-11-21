#include "option_pricer.hpp"

double OptionPricer::normal_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double OptionPricer::black_scholes_call(double S, double K, double r, double q,
                                        double sigma, double T) {
    if (S <= 0.0 || K <= 0.0 || sigma <= 0.0 || T <= 0.0) {
        return 0.0;
    }
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) /
                (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;
    double Nd1 = normal_cdf(d1);
    double Nd2 = normal_cdf(d2);
    return S * std::exp(-q * T) * Nd1 - K * std::exp(-r * T) * Nd2;
}
