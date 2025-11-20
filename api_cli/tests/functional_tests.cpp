#include "market_data_provider.hpp"
#include "price_pipe.hpp"
#include "pricing_service.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

class MockProvider : public MarketDataProvider {
public:
    PriceUpdate get_price(const std::string& ticker) override {
        // Return monotonically increasing price per ticker to make assertions easier.
        std::lock_guard<std::mutex> lock(mutex_);
        double& v = prices_[ticker];
        if (v == 0.0) {
            v = 100.0;
        } else {
            v += 1.0;
        }
        // Simulate small work to encourage thread interleaving.
        std::this_thread::sleep_for(2ms);
        PriceUpdate upd;
        upd.timestamp = ++counter_;
        upd.ticker = ticker;
        upd.price = v;
        upd.status = "OK";
        upd.error.clear();
        return upd;
    }

private:
    std::mutex mutex_;
    std::map<std::string, double> prices_;
    std::int64_t counter_{0};
};

TEST(PricingServiceFunctionalTest, MultithreadedPricingProducesDataForAllTickers) {
    auto provider = std::make_shared<MockProvider>();
    PricePipe pipe;
    std::vector<std::string> tickers = {"SBER", "GAZP", "GMKN"};

    const std::size_t num_threads = 3;
    const int interval_ms = 10;
    PricingService service(provider, tickers, pipe, num_threads, interval_ms);

    service.start();

    // Collect messages for a short period
    std::set<std::string> seen_tickers;
    int message_count = 0;

    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < 300ms) {
        PriceUpdate update;
        if (pipe.read(update)) {
            ++message_count;
            seen_tickers.insert(update.ticker);
        } else {
            break;
        }
    }

    service.stop();

    // After stopping, drain remaining messages if any.
    PriceUpdate dummy;
    while (pipe.read(dummy)) {
        ++message_count;
        seen_tickers.insert(dummy.ticker);
    }

    EXPECT_EQ(seen_tickers.size(), tickers.size());
    EXPECT_GT(message_count, static_cast<int>(tickers.size()));
}

