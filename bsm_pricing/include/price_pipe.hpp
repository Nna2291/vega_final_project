#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T> class PricePipe {
public:
  void write(const T &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      return;
    }
    queue_.push(value);
    cv_.notify_one();
  }

  bool read(T &out) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
    if (queue_.empty()) {
      return false;
    }
    out = queue_.front();
    queue_.pop();
    return true;
  }

  void close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    cv_.notify_all();
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<T> queue_;
  bool closed_{false};
};
