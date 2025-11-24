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

int main(int argc, char **argv) {
  bool test_mode = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--test") {
      test_mode = true;
    }
  }
  auto tickers = load_tickers_from_db();
  if (tickers.empty()) {
    std::cerr << "No tickers loaded from DB stub\n";
    return 1;
  }

  std::size_t threads = std::thread::hardware_concurrency() * 4;
  if (threads == 0) {
    threads = 4;
  }
  int interval_ms = 1000;

  PricePipe pipe;
  auto base_provider = std::make_shared<MoexClient>();
  std::shared_ptr<MarketDataProvider> provider = base_provider;
  if (test_mode) {
    provider = std::make_shared<RandomizedMarketDataProvider>(base_provider);
  }
  PricingService service(provider, tickers, pipe, threads, interval_ms);

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
