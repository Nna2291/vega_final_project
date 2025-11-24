#include "moex_client.hpp"
#include "price_pipe.hpp"
#include "pricing_service.hpp"
#include "randomized_provider.hpp"
#include "ticker_loader.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string get_pipe_path() {
  const char *env = std::getenv("PRICING_PIPE_PATH");
  if (env && *env) {
    return std::string(env);
  }
  return "/tmp/pricing_pipe";
}

int open_fifo_for_writing(const std::string &path) {
  // Создаём FIFO, если нужно.
  if (mkfifo(path.c_str(), 0666) < 0) {
    if (errno != EEXIST) {
      std::cerr << "Failed to create fifo at " << path << ": "
                << std::strerror(errno) << "\n";
      return -1;
    }
  }

  // Открываем на запись; вызов будет блокировать, пока кто-нибудь не откроет
  // FIFO на чтение.
  int fd = ::open(path.c_str(), O_WRONLY);
  if (fd < 0) {
    std::cerr << "Failed to open fifo for writing at " << path << ": "
              << std::strerror(errno) << "\n";
    return -1;
  }
  return fd;
}

} // namespace

struct CliConfig {
  bool test_mode{false};
  std::string pg_conninfo;
  std::string pg_host;
  std::string pg_port;
  std::string pg_user;
  std::string pg_password;
  std::string pg_db;
};

CliConfig parse_cli(int argc, char **argv) {
  CliConfig cfg;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next_string = [&](std::string &value) {
      if (i + 1 >= argc)
        return;
      value = argv[++i];
    };

    if (arg == "--test") {
      cfg.test_mode = true;
    } else if (arg == "--pg-conninfo") {
      next_string(cfg.pg_conninfo);
    } else if (arg == "--pg-host") {
      next_string(cfg.pg_host);
    } else if (arg == "--pg-port") {
      next_string(cfg.pg_port);
    } else if (arg == "--pg-user") {
      next_string(cfg.pg_user);
    } else if (arg == "--pg-password") {
      next_string(cfg.pg_password);
    } else if (arg == "--pg-db" || arg == "--pg-database") {
      next_string(cfg.pg_db);
    }
  }
  return cfg;
}

int main(int argc, char **argv) {
  CliConfig cfg = parse_cli(argc, argv);

  if (cfg.pg_conninfo.empty()) {
    if (cfg.pg_host.empty() || cfg.pg_user.empty() || cfg.pg_db.empty()) {
      std::cerr << "Missing database connection parameters. "
                << "Provide either --pg-conninfo "
                << "or all of --pg-host, --pg-user, --pg-db\n";
      return 1;
    }
    std::string ci =
        "host=" + cfg.pg_host + " user=" + cfg.pg_user + " dbname=" + cfg.pg_db;
    if (!cfg.pg_port.empty())
      ci += " port=" + cfg.pg_port;
    if (!cfg.pg_password.empty())
      ci += " password=" + cfg.pg_password;
    cfg.pg_conninfo = std::move(ci);
  }

  auto tickers = load_tickers_from_db(cfg.pg_conninfo);
  if (tickers.empty()) {
    std::cerr << "No tickers loaded from DB\n";
    return 1;
  }
  // Интервал опроса каждого тикера — 500 мс.
  int interval_ms = 500;

  PricePipe pipe;
  auto base_provider = std::make_shared<MoexClient>();
  std::shared_ptr<MarketDataProvider> provider = base_provider;
  if (cfg.test_mode) {
    provider = std::make_shared<RandomizedMarketDataProvider>(base_provider);
  }
  PricingService service(provider, tickers, pipe, interval_ms);

  const std::string pipe_path = get_pipe_path();
  int fifo_fd = open_fifo_for_writing(pipe_path);
  if (fifo_fd < 0) {
    return 1;
  }

  service.start();

  // Consumer loop: читаем из внутреннего PricePipe и отправляем JSON-строки в
  // именованный канал.
  PriceUpdate update;
  while (pipe.read(update)) {
    std::string line = "{"
                       "\"timestamp\":" +
                       std::to_string(update.timestamp) + "," +
                       "\"ticker\":\"" + update.ticker + "\"," +
                       "\"price\":" + std::to_string(update.price) + "," +
                       "\"status\":\"" + update.status + "\"," +
                       "\"error\":\"" + update.error +
                       "\""
                       "}\n";

    ssize_t written = ::write(fifo_fd, line.data(), line.size());
    std::cout << line << std::endl;
    if (written < 0) {
      std::cerr << "Failed to write to fifo " << pipe_path << ": "
                << std::strerror(errno) << "\n";
      break;
    }
  }

  ::close(fifo_fd);
  service.stop();
  return 0;
}
