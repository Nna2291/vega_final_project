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
  EXPECT_NEAR(price, 10.45, 0.1);
}

TEST(OptionPricerTest, DeepInTheMoney) {
  double S = 150.0;
  double K = 100.0;
  double r = 0.05;
  double q = 0.0;
  double sigma = 0.2;
  double T = 1.0;

  double price = OptionPricer::black_scholes_call(S, K, r, q, sigma, T);
  double intrinsic = S - K * std::exp(-r * T);
  EXPECT_GT(price, intrinsic);
  EXPECT_NEAR(price, intrinsic, 5.0);
}

TEST(OptionPricerTest, ShortMaturitySmallVol) {
  double S = 100.0;
  double K = 100.0;
  double r = 0.01;
  double q = 0.0;
  double sigma = 0.05;
  double T = 1.0 / 252.0;

  double price = OptionPricer::black_scholes_call(S, K, r, q, sigma, T);
  EXPECT_GT(price, 0.0);
  EXPECT_LT(price, 2.0);
}

TEST(PricePipeTest, WriteRead) {
  PricePipe<int> pipe;
  pipe.write(42);

  int v = 0;
  ASSERT_TRUE(pipe.read(v));
  EXPECT_EQ(v, 42);
}
