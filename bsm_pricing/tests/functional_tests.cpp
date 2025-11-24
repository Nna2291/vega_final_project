#include "bsm_service.hpp"
#include "price_pipe.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

TEST(BsmServiceFunctionalTest, ProcessesJsonAndPrintsOptionQuoteToStdout) {
  PricePipe<std::string> pipe;

  BsmService service(pipe, /*num_threads=*/2, /*conninfo=*/"");

  service.set_params_for_testing("SBER",
                                 /*K=*/100.0,
                                 /*r=*/0.05,
                                 /*q=*/0.0,
                                 /*sigma=*/0.2,
                                 /*T=*/1.0,
                                 /*ticker_id=*/1,
                                 /*conf_id=*/1);

  std::string json =
      R"({"timestamp":1700000000,"ticker":"SBER","price":100.0,"status":"OK","error":""})";

  ::testing::internal::CaptureStdout();

  service.start();
  pipe.write(json);
  pipe.close();

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  service.stop();

  std::string output = ::testing::internal::GetCapturedStdout();

  ASSERT_FALSE(output.empty());
  EXPECT_NE(output.find("\"ticker\":\"SBER\""), std::string::npos);
  EXPECT_NE(output.find("\"underlying_price\":100"), std::string::npos);
  EXPECT_NE(output.find("\"status\":\"OK\""), std::string::npos);

  auto pos = output.find("\"option_price\":");
  ASSERT_NE(pos, std::string::npos);
}
