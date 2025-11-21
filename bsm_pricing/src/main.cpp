#include "bsm_service.hpp"
#include "influx_writer.hpp"
#include "messages.hpp"
#include "price_pipe.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace {

void load_env_from_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Обрезаем пробелы по краям
        auto trim = [](std::string& s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
                s.erase(s.begin());
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
                s.pop_back();
            }
        };
        trim(key);
        trim(value);

        if (key.empty()) {
            continue;
        }

        // Не перезаписываем уже существующие переменные окружения.
        if (std::getenv(key.c_str()) == nullptr) {
            setenv(key.c_str(), value.c_str(), 0);
        }
    }
}

std::string get_pipe_path() {
    const char* env = std::getenv("PRICING_PIPE_PATH");
    if (env && *env) {
        return std::string(env);
    }
    return "/tmp/pricing_pipe";
}

int open_fifo_for_reading(const std::string& path) {
    // FIFO должен быть создан продюсером (api_cli). Здесь только открываем.
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open fifo for reading at " << path << ": "
                  << std::strerror(errno) << "\n";
        return -1;
    }
    return fd;
}

bool parse_price_update(const std::string& line, PriceUpdateIn& out) {
    // Очень простой парсер под формат:
    // {"timestamp":..., "ticker":"...", "price":..., "status":"...", "error":"..."}
    try {
        auto get_value = [&](const std::string& key) -> std::string {
            auto pos = line.find("\"" + key + "\"");
            if (pos == std::string::npos) return {};
            pos = line.find(':', pos);
            if (pos == std::string::npos) return {};
            ++pos;
            // пропустим пробелы
            while (pos < line.size() && (line[pos] == ' ')) ++pos;
            std::size_t end = pos;
            if (line[pos] == '\"') {
                ++pos;
                end = line.find('\"', pos);
                if (end == std::string::npos) return {};
                return line.substr(pos, end - pos);
            } else {
                // число до запятой или закрывающей скобки
                end = pos;
                while (end < line.size() && line[end] != ',' && line[end] != '}') {
                    ++end;
                }
                return line.substr(pos, end - pos);
            }
        };

        std::string ts_str = get_value("timestamp");
        std::string ticker = get_value("ticker");
        std::string price_str = get_value("price");
        std::string status = get_value("status");
        std::string error = get_value("error");

        if (ticker.empty()) return false;

        out.timestamp = ts_str.empty() ? 0 : std::stoll(ts_str);
        out.ticker = ticker;
        out.price = price_str.empty() ? 0.0 : std::stod(price_str);
        out.status = status.empty() ? "ERROR" : status;
        out.error = error;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int main() {
    // Загружаем переменные окружения из .env (если файл существует).
    // Приоритет: ENV_FILE -> .env в текущем каталоге -> ../.env (для запуска из build/).
    if (const char* env_file = std::getenv("ENV_FILE")) {
        if (*env_file) {
            load_env_from_file(env_file);
        }
    } else {
        load_env_from_file(".env");
        load_env_from_file("../.env");
    }

    const std::string pipe_path = get_pipe_path();
    int fifo_fd = open_fifo_for_reading(pipe_path);
    if (fifo_fd < 0) {
        return 1;
    }

    PricePipe<PriceUpdateIn> in_pipe;
    PricePipe<OptionQuote> out_pipe;

    double strike = 300.0;
    double rate = 0.10;
    double dividend_yield = 0.02;
    double volatility = 0.25;
    double maturity_years = 0.5;

    std::size_t threads = std::thread::hardware_concurrency();
    if (threads == 0) {
        threads = 4;
    }

    BsmService service(in_pipe, out_pipe, threads, strike, rate, dividend_yield,
                       volatility, maturity_years);
    service.start();

    InfluxWriter writer;
    if (!writer.is_configured()) {
        std::cerr << "InfluxWriter is not configured, results will be printed to stdout\n";
    }

    // Поток-консьюмер: читает котировки опционов и пишет их в InfluxDB или stdout.
    std::thread consumer([&out_pipe, &writer] {
        OptionQuote q{};
        while (out_pipe.read(q)) {
            if (writer.is_configured()) {
                writer.write(q);
            } else {
                std::cout << "{"
                          << "\"timestamp\":" << q.timestamp << ","
                          << "\"ticker\":\"" << q.ticker << "\","
                          << "\"underlying_price\":" << q.underlying_price << ","
                          << "\"option_price\":" << q.option_price << ","
                          << "\"status\":\"" << q.status << "\","
                          << "\"error\":\"" << q.error << "\""
                          << "}\n";
            }
        }
    });

    // Главный поток читает строки из именованного канала и кладёт в in_pipe.
    FILE* f = fdopen(fifo_fd, "r");
    if (!f) {
        std::cerr << "fdopen failed\n";
        in_pipe.close();
        service.stop();
        out_pipe.close();
        consumer.join();
        ::close(fifo_fd);
        return 1;
    }

    char* lineptr = nullptr;
    size_t n = 0;
    while (true) {
        ssize_t len = getline(&lineptr, &n, f);
        if (len <= 0) {
            break;
        }
        std::string line(lineptr, static_cast<std::size_t>(len));
        PriceUpdateIn msg{};
        if (parse_price_update(line, msg)) {
            in_pipe.write(msg);
        }
    }
    free(lineptr);

    fclose(f); // закрывает и fifo_fd
    in_pipe.close();
    service.stop();
    out_pipe.close();
    consumer.join();
    return 0;
}


