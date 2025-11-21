#include "moex_client.hpp"
#include "price_pipe.hpp"
#include "pricing_service.hpp"
#include "ticker_loader.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <mutex>
#include <set>

// Sample response similar to
// https://iss.moex.com/iss/engines/stock/markets/shares/boards/tqbr/securities/sber.json?iss.meta=off
static const char* SAMPLE_JSON = R"json(
{
  "marketdata": {
    "columns": ["SECID","BOARDID","BID","BIDDEPTH","OFFER","OFFERDEPTH","SPREAD","BIDDEPTHT","OFFERDEPTHT","OPEN","LOW","HIGH","LAST","LASTCHANGE","LASTCHANGEPRCNT","QTY","VALUE","VALUE_USD","WAPRICE","LASTCNGTOLASTWAPRICE","WAPTOPREVWAPRICEPRCNT","WAPTOPREVWAPRICE","CLOSEPRICE","MARKETPRICETODAY","MARKETPRICE","LASTTOPREVPRICE","NUMTRADES","VOLTODAY","VALTODAY","VALTODAY_USD","ETFSETTLEPRICE","TRADINGSTATUS","UPDATETIME","LASTBID","LASTOFFER","LCLOSEPRICE","LCURRENTPRICE","MARKETPRICE2","NUMBIDS","NUMOFFERS","CHANGE","TIME","HIGHBID","LOWOFFER","PRICEMINUSPREVWAPRICE","OPENPERIODPRICE","SEQNUM","SYSTIME","CLOSINGAUCTIONPRICE","CLOSINGAUCTIONVOLUME","ISSUECAPITALIZATION","ISSUECAPITALIZATION_UPDATETIME","ETFSETTLECURRENCY","VALTODAY_RUR","TRADINGSESSION","TRENDISSUECAPITALIZATION"],
    "data": [
      ["SBER","TQBR",302.92,null,302.93,null,0.01,3204303,4531837,304.76,302.78,307.24,302.92,-0.01,0,104,31503.68,389.2,304.74,-1.97,-0.05,-0.15,null,null,304.73,-0.6,94909,15287987,4658931361,57556895,null,"T","15:25:37",null,null,null,302.96,null,null,null,-1.83,"15:25:36",null,null,-1.97,304.76,20251120154037,"2025-11-20 15:40:37",null,null,6542788069320,"15:39:59",null,4658931361,"1",-35834333680]
    ]
  }
}
)json";

class TestMoexClient : public MoexClient {
protected:
    std::string http_get(const std::string& /*url*/) const override {
        return SAMPLE_JSON;
    }
};

TEST(MoexClientTest, ParseLastPrice) {
    double price = MoexClient::parse_last_price_from_json(SAMPLE_JSON);
    EXPECT_DOUBLE_EQ(price, 302.92);
}

TEST(MoexClientTest, GetPriceUsesHttpAndParse) {
    TestMoexClient client;
    PriceUpdate upd = client.get_price("SBER");
    EXPECT_EQ(upd.ticker, "SBER");
    EXPECT_DOUBLE_EQ(upd.price, 302.92);
    // 2025-11-20 15:40:37 UTC -> 1763653237
    EXPECT_EQ(upd.timestamp, 1763653237);
    EXPECT_EQ(upd.status, "OK");
    EXPECT_TRUE(upd.error.empty());
}

// Дополнительный интеграционный тест, который ходит на реальный сервис MOEX ISS
// по адресу вида:
// https://iss.moex.com/iss/engines/stock/markets/shares/boards/tqbr/securities/sber.json?iss.meta=off
// Тест может быть нестабильным при отсутствии сети, поэтому при желании его можно
// отключить через опции запуска gtest (filter) или env.
TEST(MoexClientRealTest, FetchesSberFromRealMoex) {
    MoexClient client;
    PriceUpdate upd = client.get_price("SBER");
    EXPECT_EQ(upd.ticker, "SBER");
    EXPECT_EQ(upd.status, "OK");
    EXPECT_TRUE(upd.error.empty());
    EXPECT_GT(upd.price, 0.0);
    EXPECT_GT(upd.timestamp, 0);
}

TEST(PricePipeTest, BasicWriteRead) {
    PricePipe pipe;
    PriceUpdate in{1234567890, "SBER", 100.5};
    in.status = "OK";

    pipe.write(in);

    PriceUpdate out{};
    bool ok = pipe.read(out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out.timestamp, in.timestamp);
    EXPECT_EQ(out.ticker, in.ticker);
    EXPECT_DOUBLE_EQ(out.price, in.price);
}

TEST(PricePipeTest, CloseStopsReaders) {
    PricePipe pipe;
    pipe.close();
    PriceUpdate out{};

    bool ok = pipe.read(out);
    EXPECT_FALSE(ok);
}

TEST(TickerLoaderTest, StubReturnsNonEmpty) {
    auto tickers = load_tickers_from_db();
    EXPECT_FALSE(tickers.empty());
}

class PricingServiceMockProvider : public MarketDataProvider {
public:
    PriceUpdate get_price(const std::string& ticker) override {
        std::lock_guard<std::mutex> lock(mutex_);
        double& v = prices_[ticker];
        if (v == 0.0) {
            v = 100.0;
        } else {
            v += 1.0;
        }
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

class FailingProvider : public MarketDataProvider {
public:
    PriceUpdate get_price(const std::string& ticker) override {
        throw std::runtime_error("network error");
    }
};

TEST(PricingServiceTest, ProducesUpdatesForTickers) {
    auto provider = std::make_shared<PricingServiceMockProvider>();
    PricePipe pipe;
    std::vector<std::string> initial_tickers = {"AAA", "BBB", "CCC"};

    PricingService service(provider, initial_tickers, pipe, 2, 10);
    service.start();

    std::set<std::string> seen_tickers;
    const int kMessagesToRead = 10;

    for (int i = 0; i < kMessagesToRead; ++i) {
        PriceUpdate upd;
        ASSERT_TRUE(pipe.read(upd));
        if (upd.status == "OK") {
            seen_tickers.insert(upd.ticker);
            EXPECT_GT(upd.price, 0.0);
        }
    }

    service.stop();

    // После остановки возможны дополнительные сообщения в очереди — вычитаем их.
    PriceUpdate upd;
    while (pipe.read(upd)) {
        if (upd.status == "OK") {
            seen_tickers.insert(upd.ticker);
        }
    }

    EXPECT_FALSE(seen_tickers.empty());
}

TEST(PricingServiceTest, EmitsErrorOnException) {
    auto provider = std::make_shared<FailingProvider>();
    PricePipe pipe;
    std::vector<std::string> initial_tickers = {"ERR_TICK"};

    PricingService service(provider, initial_tickers, pipe, 1, 10);
    service.start();

    PriceUpdate upd;
    ASSERT_TRUE(pipe.read(upd));
    EXPECT_EQ(upd.ticker, "ERR_TICK");
    EXPECT_EQ(upd.status, "ERROR");
    EXPECT_FALSE(upd.error.empty());

    service.stop();
}

