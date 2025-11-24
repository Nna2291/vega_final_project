#pragma once

#include <cstdint>
#include <string>

struct PriceUpdate {
  std::int64_t timestamp{};
  std::string ticker;
  double price{};
  std::string status;
  std::string error;
};
