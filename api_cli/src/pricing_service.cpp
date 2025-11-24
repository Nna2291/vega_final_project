#include "pricing_service.hpp"

#include "price_update.hpp"
#include "ticker_loader.hpp"

#include <chrono>

PricingService::PricingService(std::shared_ptr<MarketDataProvider> provider,
                               std::vector<std::string> tickers,
                               PricePipe &pipe, int interval_ms)
    : provider_(std::move(provider)), pipe_(pipe), interval_ms_(interval_ms),
      tickers_(std::move(tickers)) {}

PricingService::~PricingService() { stop(); }

void PricingService::start() {
  if (running_.exchange(true)) {
    return;
  }

  threads_.clear();
  threads_.reserve(tickers_.size());
  for (const auto &t : tickers_) {
    threads_.emplace_back(&PricingService::worker_thread, this, t);
  }
}

void PricingService::stop() {
  if (!running_.exchange(false)) {
    return;
  }

  for (auto &t : threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  threads_.clear();
  pipe_.close();
}

void PricingService::worker_thread(std::string ticker) {
  using namespace std::chrono;
  const auto sleep_duration = std::chrono::milliseconds(interval_ms_);

  std::int64_t last_ts = -1;

  while (running_) {
    try {
      PriceUpdate update = provider_->get_price(ticker);

      bool should_emit = false;
      if (update.status == "OK") {
        if (update.timestamp > last_ts) {
          last_ts = update.timestamp;
          should_emit = true;
        }
      } else if (update.status == "ERROR") {
        should_emit = true;
      }

      if (should_emit) {
        pipe_.write(update);
      }
    } catch (const std::exception &ex) {
      PriceUpdate err;
      err.timestamp = -1;
      err.ticker = ticker;
      err.price = 0.0;
      err.status = "ERROR";
      err.error = ex.what();
      pipe_.write(err);
    } catch (...) {
      PriceUpdate err;
      err.timestamp = -1;
      err.ticker = ticker;
      err.price = 0.0;
      err.status = "ERROR";
      err.error = "Unknown error during price fetch";
      pipe_.write(err);
    }

    std::this_thread::sleep_for(sleep_duration);
  }
}
