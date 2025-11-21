#pragma once

#include <cstdint>
#include <string>

// Входящее сообщение из сервиса цен (api_cli).
struct PriceUpdateIn {
    std::int64_t timestamp{};
    std::string ticker;
    double price{};
    std::string status;
    std::string error;
};

// Результат прайсинга опциона по BSM.
struct OptionQuote {
    std::int64_t timestamp{};
    std::string ticker;
    double underlying_price{};
    double option_price{};
    std::string status;
    std::string error;
};


