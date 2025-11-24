#pragma once

#include <string>

#include "price_update.hpp"

class MarketDataProvider {
public:
  virtual ~MarketDataProvider() = default;

  virtual PriceUpdate get_price(const std::string &ticker) = 0;
};
