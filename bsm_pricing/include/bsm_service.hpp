#pragma once

#include "messages.hpp"
#include "option_pricer.hpp"
#include "price_pipe.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Многопоточный сервис BSM: читает PriceUpdateIn из входного пайпа
// и пишет OptionQuote в выходной пайп.
class BsmService {
public:
    BsmService(PricePipe<PriceUpdateIn>& in_pipe,
               PricePipe<OptionQuote>& out_pipe,
               std::size_t num_threads,
               double strike,
               double rate,
               double dividend_yield,
               double volatility,
               double maturity_years);

    ~BsmService();

    void start();
    void stop();

private:
    void worker_thread();

    PricePipe<PriceUpdateIn>& in_pipe_;
    PricePipe<OptionQuote>& out_pipe_;
    std::size_t num_threads_;

    double K_;
    double r_;
    double q_;
    double sigma_;
    double T_;

    std::atomic<bool> running_{false};
    std::vector<std::thread> threads_;
};


