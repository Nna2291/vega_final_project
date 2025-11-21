#include "bsm_service.hpp"

BsmService::BsmService(PricePipe<PriceUpdateIn>& in_pipe,
                       PricePipe<OptionQuote>& out_pipe,
                       std::size_t num_threads,
                       double strike,
                       double rate,
                       double dividend_yield,
                       double volatility,
                       double maturity_years)
    : in_pipe_(in_pipe),
      out_pipe_(out_pipe),
      num_threads_(num_threads),
      K_(strike),
      r_(rate),
      q_(dividend_yield),
      sigma_(volatility),
      T_(maturity_years) {}

BsmService::~BsmService() {
    stop();
}

void BsmService::start() {
    if (running_.exchange(true)) {
        return;
    }
    threads_.clear();
    threads_.reserve(num_threads_);
    for (std::size_t i = 0; i < num_threads_; ++i) {
        threads_.emplace_back(&BsmService::worker_thread, this);
    }
}

void BsmService::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
    out_pipe_.close();
}

void BsmService::worker_thread() {
    while (running_) {
        PriceUpdateIn in{};
        if (!in_pipe_.read(in)) {
            break;
        }

        OptionQuote out{};
        out.timestamp = in.timestamp;
        out.ticker = in.ticker;
        out.underlying_price = in.price;

        if (in.status != "OK") {
            out.status = "ERROR";
            out.error = in.error.empty() ? "Upstream price error" : in.error;
        } else {
            double call_price =
                OptionPricer::black_scholes_call(in.price, K_, r_, q_, sigma_, T_);
            out.option_price = call_price;
            out.status = "OK";
            out.error.clear();
        }

        out_pipe_.write(out);
    }
}


