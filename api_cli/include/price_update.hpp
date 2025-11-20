#pragma once

#include <cstdint>
#include <string>

struct PriceUpdate {
    // Биржевой timestamp в секундах от эпохи (UTC), вычисленный из поля SYSTIME.
    std::int64_t timestamp{};
    std::string ticker;
    double price{};

    // Статус получения котировки: "OK" или "ERROR".
    std::string status;
    // Текст ошибки, если статус "ERROR", иначе пустая строка.
    std::string error;
};


