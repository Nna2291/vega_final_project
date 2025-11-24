#include "price_pipe.hpp"
#include "pricing_service.hpp"
#include "randomized_provider.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <set>
#include <thread>

class FunctionalConstantProvider : public MarketDataProvider {
public:
  FunctionalConstantProvider(double price, std::int64_t ts)
      : price_(price), ts_(ts) {}

  PriceUpdate get_price(const std::string &ticker) override {
    PriceUpdate upd;
    upd.timestamp = ts_;
    upd.ticker = ticker;
    upd.price = price_;
    upd.status = "OK";
    upd.error.clear();
    return upd;
  }

private:
  double price_;
  std::int64_t ts_;
};

TEST(PricingServiceFunctionalTest, ProducesRandomizedPricesForAllTickers) {
  auto base = std::make_shared<FunctionalConstantProvider>(100.0, 1'000'000);
  auto provider = std::make_shared<RandomizedMarketDataProvider>(base);

  PriceQueue pipe;
  std::vector<std::string> tickers = {"AAA", "BBB", "CCC"};

  PricingService service(provider, tickers, pipe, /*interval_ms=*/5);
  service.start();

  std::set<std::string> seen_ok_tickers;

  const int kMessagesToRead = 30;
  for (int i = 0; i < kMessagesToRead; ++i) {
    PriceUpdate upd;
    ASSERT_TRUE(pipe.read(upd));
    if (upd.status == "OK") {
      seen_ok_tickers.insert(upd.ticker);
      EXPECT_GE(upd.price, 90.0);
      EXPECT_LE(upd.price, 110.0);
    }
  }

  service.stop();

  EXPECT_EQ(seen_ok_tickers.size(), tickers.size());
  for (const auto &t : tickers) {
    EXPECT_TRUE(seen_ok_tickers.count(t));
  }
}
