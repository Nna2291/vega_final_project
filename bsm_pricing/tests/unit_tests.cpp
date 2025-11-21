#include "bsm_service.hpp"
#include "messages.hpp"
#include "option_pricer.hpp"
#include "price_pipe.hpp"

#include <gtest/gtest.h>

#include <thread>

TEST(OptionPricerTest, BlackScholesCallBasic) {
    double S = 100.0;
    double K = 100.0;
    double r = 0.05;
    double q = 0.0;
    double sigma = 0.2;
    double T = 1.0;

    double price = OptionPricer::black_scholes_call(S, K, r, q, sigma, T);
    // Известное значение для этих параметров ~10.45
    EXPECT_NEAR(price, 10.45, 0.1);
}

TEST(PricePipeTest, WriteRead) {
    PricePipe<int> pipe;
    pipe.write(42);

    int v = 0;
    ASSERT_TRUE(pipe.read(v));
    EXPECT_EQ(v, 42);
}

TEST(BsmServiceTest, ProducesOptionQuotes) {
    PricePipe<PriceUpdateIn> in_pipe;
    PricePipe<OptionQuote> out_pipe;

    double strike = 100.0;
    double r = 0.05;
    double q = 0.0;
    double sigma = 0.2;
    double T = 1.0;

    BsmService service(in_pipe, out_pipe, 2, strike, r, q, sigma, T);
    service.start();

    PriceUpdateIn in{};
    in.timestamp = 123;
    in.ticker = "TEST";
    in.price = 100.0;
    in.status = "OK";
    in_pipe.write(in);
    in_pipe.close();

    OptionQuote out{};
    ASSERT_TRUE(out_pipe.read(out));
    EXPECT_EQ(out.ticker, "TEST");
    EXPECT_EQ(out.timestamp, 123);
    EXPECT_EQ(out.status, "OK");
    EXPECT_GT(out.option_price, 0.0);

    service.stop();
}

TEST(BsmServiceTest, PropagatesErrorStatus) {
    PricePipe<PriceUpdateIn> in_pipe;
    PricePipe<OptionQuote> out_pipe;

    BsmService service(in_pipe, out_pipe, 1, 100.0, 0.05, 0.0, 0.2, 1.0);
    service.start();

    PriceUpdateIn in{};
    in.timestamp = 0;
    in.ticker = "ERR";
    in.price = 0.0;
    in.status = "ERROR";
    in.error = "upstream error";
    in_pipe.write(in);
    in_pipe.close();

    OptionQuote out{};
    ASSERT_TRUE(out_pipe.read(out));
    EXPECT_EQ(out.ticker, "ERR");
    EXPECT_EQ(out.status, "ERROR");
    EXPECT_EQ(out.error, "upstream error");

    service.stop();
}


