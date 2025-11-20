#include "pricing_service.hpp"

#include "price_update.hpp"
#include "ticker_loader.hpp"

#include <chrono>

PricingService::PricingService(std::shared_ptr<MarketDataProvider> provider,
                               std::vector<std::string> tickers,
                               PricePipe& pipe,
                               std::size_t num_threads,
                               int interval_ms)
    : provider_(std::move(provider)),
      pipe_(pipe),
      num_threads_(num_threads),
      interval_ms_(interval_ms),
      reload_interval_ms_(interval_ms * 5),
      tickers_(std::move(tickers)) {
    // Инициализируем последние времена для начального набора тикеров.
    for (const auto& t : tickers_) {
        last_timestamps_[t] = -1;
    }
}

PricingService::~PricingService() {
    stop();
}

void PricingService::start() {
    if (running_.exchange(true)) {
        return;
    }

    // Запускаем рабочие потоки.
    threads_.clear();
    threads_.reserve(num_threads_);
    for (std::size_t i = 0; i < num_threads_; ++i) {
        threads_.emplace_back(&PricingService::worker_thread, this, i);
    }

    // Запускаем поток-диспетчер, который периодически перечитывает тикеры из БД.
    dispatcher_ = std::thread(&PricingService::dispatcher_thread, this);
}

void PricingService::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (dispatcher_.joinable()) {
        dispatcher_.join();
    }

    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
    pipe_.close();
}

void PricingService::dispatcher_thread() {
    using namespace std::chrono;
    const auto sleep_duration =
        std::chrono::milliseconds(reload_interval_ms_ > 0 ? reload_interval_ms_ : 1000);

    while (running_) {
        // Загружаем актуальный список тикеров из БД.
        auto db_tickers = load_tickers_from_db();

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (db_tickers != tickers_) {
                // Перестраиваем last_timestamps_: переносим значения для
                // сохранившихся тикеров, для новых ставим -1.
                std::unordered_map<std::string, std::int64_t> new_last;
                for (const auto& t : db_tickers) {
                    auto it = last_timestamps_.find(t);
                    if (it != last_timestamps_.end()) {
                        new_last[t] = it->second;
                    } else {
                        new_last[t] = -1;
                    }
                }
                tickers_ = std::move(db_tickers);
                last_timestamps_ = std::move(new_last);
            }
        }

        std::this_thread::sleep_for(sleep_duration);
    }
}

void PricingService::worker_thread(std::size_t worker_index) {
    using namespace std::chrono;
    const auto sleep_duration = std::chrono::milliseconds(interval_ms_);

    while (running_) {
        // Снимаем локальную копию списка тикеров, чтобы уменьшить время под мьютексом.
        std::vector<std::string> local_tickers;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            local_tickers = tickers_;
        }

        for (std::size_t i = worker_index; i < local_tickers.size(); i += num_threads_) {
            const auto& ticker = local_tickers[i];
            try {
                PriceUpdate update = provider_->get_price(ticker);

                bool should_emit = false;
                if (update.status == "OK") {
                    // Проверяем и обновляем последний timestamp для данного тикера.
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    auto& last_ts = last_timestamps_[ticker];
                    if (update.timestamp > last_ts) {
                        last_ts = update.timestamp;
                        should_emit = true;
                    }
                } else if (update.status == "ERROR") {
                    // Ошибки всегда пробрасываем дальше.
                    should_emit = true;
                }

                if (should_emit) {
                    pipe_.write(update);
                }
            } catch (const std::exception& ex) {
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
        }

        std::this_thread::sleep_for(sleep_duration);
    }
}

