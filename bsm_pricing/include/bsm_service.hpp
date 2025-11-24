#pragma once

#include "messages.hpp"
#include "option_pricer.hpp"
#include "postgres_writer.hpp"
#include "price_pipe.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Многопоточный сервис: из json_pipe берёт сырые JSON-строки,
// парсит PriceUpdateIn, считывает конфигурацию BSM из PostgreSQL
// и, если конфигурация для тикера найдена, считает BSM-цену опциона
// и записывает результат в PostgreSQL.
class BsmService {
public:
  BsmService(PricePipe<std::string> &json_pipe, std::size_t num_threads,
             const std::string &conninfo);

  ~BsmService();

  void start();
  void stop();

private:
  struct BsmParams {
    double K;
    double r;
    double q;
    double sigma;
    double T;
    long long ticker_id;
    long long conf_id;
  };

  void worker_thread();
  void dispatcher_thread();
  void config_thread();
  void db_thread();

  PricePipe<std::string> &json_pipe_;

  std::queue<std::string> queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  bool queue_closed_{false};

  // Очередь для рассчитанных котировок, которые должен записывать один поток в
  // БД.
  std::queue<OptionQuote> out_queue_;
  std::mutex out_mutex_;
  std::condition_variable out_cv_;
  bool out_closed_{false};
  std::atomic<std::size_t> out_queue_size_{0};

  std::size_t num_threads_;

  std::unordered_map<std::string, BsmParams> params_;
  std::mutex params_mutex_;

  std::string conninfo_;

  int reload_interval_sec_{5};

  std::atomic<bool> running_{false};
  std::vector<std::thread> threads_;
  std::thread dispatcher_thread_;
  std::thread db_thread_;
  std::thread config_thread_;
};
