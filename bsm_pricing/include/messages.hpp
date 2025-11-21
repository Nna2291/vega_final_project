#pragma once

#include <cstdint>
#include <string>

// Входящее сообщение из сервиса цен
struct PriceUpdateIn {
  std::int64_t timestamp{};
  std::string ticker;
  double price{};
  std::string status;
  std::string error;
};

// Результат прайсинга опциона
struct OptionQuote {
  std::int64_t timestamp{};
  std::string ticker;
  double underlying_price{};
  double option_price{};
  long long ticker_id{};
  long long conf_id{};
  std::string status;
  std::string error;
};
