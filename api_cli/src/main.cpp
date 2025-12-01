#include "cli_config.hpp"
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
#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
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
  if (mkfifo(path.c_str(), 0666) < 0) {
    if (errno != EEXIST) {
      std::cerr << "Failed to create fifo at " << path << ": "
                << std::strerror(errno) << "\n";
      return -1;
    }
  }

  int fd = ::open(path.c_str(), O_WRONLY);
  if (fd < 0) {
    std::cerr << "Failed to open fifo for writing at " << path << ": "
              << std::strerror(errno) << "\n";
    return -1;
  }
  return fd;
}

} // namespace

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
  int interval_ms = 500;

  PriceQueue pipe;
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

  // Фоновый поток, который периодически перечитывает список тикеров из БД
  // и добавляет новые тикеры в PricingService без перезапуска процесса.
  std::atomic<bool> reload_running{true};
  std::thread reload_thread([&]() {
    using namespace std::chrono_literals;
    std::vector<std::string> known = tickers;
    while (reload_running.load()) {
      std::this_thread::sleep_for(5s);

      auto fresh = load_tickers_from_db(cfg.pg_conninfo);
      if (fresh.empty()) {
        continue;
      }

      std::vector<std::string> to_add;
      for (const auto &t : fresh) {
        if (std::find(known.begin(), known.end(), t) == known.end()) {
          known.push_back(t);
          to_add.push_back(t);
        }
      }

      if (!to_add.empty()) {
        service.add_tickers(to_add);
      }
    }
  });
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

  reload_running.store(false);
  if (reload_thread.joinable()) {
    reload_thread.join();
  }

  ::close(fifo_fd);
  service.stop();
  return 0;
}
