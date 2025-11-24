#include "randomized_provider.hpp"

#include <random>

RandomizedMarketDataProvider::RandomizedMarketDataProvider(
    std::shared_ptr<MarketDataProvider> base)
    : base_(std::move(base)) {}

PriceUpdate RandomizedMarketDataProvider::get_price(const std::string &ticker) {
  PriceUpdate base = base_->get_price(ticker);
  if (base.status != "OK") {
    return base;
  }

  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<double> dist(-0.10, 0.10);
  double delta = dist(rng);

  double new_price = base.price * (1.0 + delta);
  if (new_price < 0.0) {
    new_price = 0.0;
  }
  base.price = new_price;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = last_simulated_ts_.find(ticker);

    std::int64_t simulated_ts = base.timestamp;
    if (it == last_simulated_ts_.end()) {
      if (simulated_ts <= 0) {
        simulated_ts = static_cast<std::int64_t>(std::time(nullptr));
      }
    } else {
      if (base.timestamp > it->second) {
        simulated_ts = base.timestamp;
      } else {
        simulated_ts = it->second + 1;
      }
    }

    last_simulated_ts_[ticker] = simulated_ts;
    base.timestamp = simulated_ts;
  }

  return base;
}
