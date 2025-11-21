#pragma once

#include "messages.hpp"

#include <curl/curl.h>

#include <string>

// Простой писатель в InfluxDB по HTTP (InfluxDB 2.x API).
// Использует переменные окружения:
//   INFLUX_URL   - базовый URL, например http://localhost:8086
//   INFLUX_ORG   - org
//   INFLUX_BUCKET- bucket
//   INFLUX_TOKEN - API токен
//   INFLUX_MEASUREMENT - (необязательно) имя измерения, сейчас тикер используется
//                        как имя измерения для удобства чтения данных
class InfluxWriter {
public:
    InfluxWriter();
    ~InfluxWriter();

    // Возвращает true при успешной отправке.
    bool write(const OptionQuote& quote);

    bool is_configured() const { return configured_; }

private:
    std::string url_;          // полный URL /api/v2/write?...
    std::string token_;        // Authorization: Token <token>
    std::string measurement_;  // зарезервировано, сейчас measurement = ticker
    bool configured_{false};

    CURL* curl_{nullptr};

    std::string build_line_protocol(const OptionQuote& q) const;
};


