#pragma once

#include <string>
#include <vector>

// Загрузка списка тикеров из PostgreSQL по строке подключения conninfo.
std::vector<std::string> load_tickers_from_db(const std::string &conninfo);
