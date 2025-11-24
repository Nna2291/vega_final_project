#include "price_pipe.hpp"

void PriceQueue::write(const PriceUpdate &update) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_) {
    return;
  }
  queue_.push(update);
  cv_.notify_one();
}

bool PriceQueue::read(PriceUpdate &out) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
  if (queue_.empty()) {
    return false;
  }
  out = queue_.front();
  queue_.pop();
  return true;
}

void PriceQueue::close() {
  std::lock_guard<std::mutex> lock(mutex_);
  closed_ = true;
  cv_.notify_all();
}
