#pragma once

#include <string>

#include "price_update.hpp"

// Abstract interface to obtain market prices for tickers.
class MarketDataProvider {
public:
    virtual ~MarketDataProvider() = default;

    // Возвращает полное обновление цены, включающее биржевой timestamp.
    virtual PriceUpdate get_price(const std::string& ticker) = 0;
};


