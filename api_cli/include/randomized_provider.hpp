#pragma once

#include "market_data_provider.hpp"

#include <memory>
#include <mutex>
#include <unordered_map>

class RandomizedMarketDataProvider : public MarketDataProvider {
public:
  explicit RandomizedMarketDataProvider(
      std::shared_ptr<MarketDataProvider> base);

  PriceUpdate get_price(const std::string &ticker) override;

private:
  std::shared_ptr<MarketDataProvider> base_;

  std::unordered_map<std::string, std::int64_t> last_simulated_ts_;
  std::mutex mutex_;
};
