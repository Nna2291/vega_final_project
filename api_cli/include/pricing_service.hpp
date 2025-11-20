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

// Multithreaded pricing service: fetches prices for configured tickers
// using provided MarketDataProvider and writes PriceUpdate messages into
// a PricePipe.
class PricingService {
public:
    PricingService(std::shared_ptr<MarketDataProvider> provider,
                   std::vector<std::string> tickers,
                   PricePipe& pipe,
                   std::size_t num_threads,
                   int interval_ms);

    ~PricingService();

    void start();
    void stop();

private:
    void worker_thread(std::size_t worker_index);
    void dispatcher_thread();

    std::shared_ptr<MarketDataProvider> provider_;
    PricePipe& pipe_;
    std::size_t num_threads_;
    int interval_ms_;
    int reload_interval_ms_;

    // Общий для сервиса набор тикеров и последние времена по каждому.
    std::vector<std::string> tickers_;
    std::unordered_map<std::string, std::int64_t> last_timestamps_;
    std::mutex state_mutex_;

    std::atomic<bool> running_{false};
    std::vector<std::thread> threads_;
    std::thread dispatcher_;
};


