#pragma once

#include "market_data_provider.hpp"
#include "price_pipe.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class PricingService {
public:
  PricingService(std::shared_ptr<MarketDataProvider> provider,
                 std::vector<std::string> tickers, PriceQueue &pipe,
                 int interval_ms);

  ~PricingService();

  void start();
  void stop();

  // Добавить новые тикеры во время работы сервиса.
  // Для каждого нового тикера запускается отдельный поток.
  void add_tickers(const std::vector<std::string> &tickers);

private:
  void worker_thread(std::string ticker);

  std::shared_ptr<MarketDataProvider> provider_;
  PriceQueue &pipe_;
  int interval_ms_;

  std::vector<std::string> tickers_;

  std::mutex mutex_;
  std::atomic<bool> running_{false};
  std::vector<std::thread> threads_;
};
