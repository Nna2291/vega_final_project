#pragma once

#include "market_data_provider.hpp"
#include "price_pipe.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class PricingService {
public:
  PricingService(std::shared_ptr<MarketDataProvider> provider,
                 std::vector<std::string> tickers, PricePipe &pipe,
                 int interval_ms);

  ~PricingService();

  void start();
  void stop();

private:
  void worker_thread(std::string ticker);

  std::shared_ptr<MarketDataProvider> provider_;
  PricePipe &pipe_;
  int interval_ms_;

  std::vector<std::string> tickers_;

  std::atomic<bool> running_{false};
  std::vector<std::thread> threads_;
};
