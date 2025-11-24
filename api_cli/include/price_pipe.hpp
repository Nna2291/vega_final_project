#pragma once

#include "price_update.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>

class PriceQueue {
public:
  void write(const PriceUpdate &update);

  bool read(PriceUpdate &out);

  void close();

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<PriceUpdate> queue_;
  bool closed_{false};
};
