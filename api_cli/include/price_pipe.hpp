#pragma once

#include "price_update.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>

// Thread-safe in-process pipe for PriceUpdate messages.
class PricePipe {
public:
    void write(const PriceUpdate& update);

    // Blocking read. Returns false if pipe is closed and queue is empty.
    bool read(PriceUpdate& out);

    // Close pipe: wake up readers and stop accepting new data.
    void close();

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<PriceUpdate> queue_;
    bool closed_{false};
};


