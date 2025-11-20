#pragma once

#include "market_data_provider.hpp"

#include <string>

// Concrete implementation of MarketDataProvider using MOEX ISS HTTP API.
class MoexClient : public MarketDataProvider {
public:
    MoexClient();
    ~MoexClient() override;

    // Получает полное обновление цены по тикеру: цена + биржевой timestamp (SEQNUM).
    PriceUpdate get_price(const std::string& ticker) override;

    // Вспомогательная функция для тестов: парсит JSON и возвращает только LAST.
    static double parse_last_price_from_json(const std::string& body);

    // Полный парсинг обновления из JSON: LAST + SEQNUM.
    static PriceUpdate parse_update_from_json(const std::string& body,
                                              const std::string& ticker);

protected:
    // HTTP request, made virtual to allow overriding in tests.
    virtual std::string http_get(const std::string& url) const;

private:
    std::string build_url(const std::string& ticker) const;
};

