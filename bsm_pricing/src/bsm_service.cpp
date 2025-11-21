#include "bsm_service.hpp"

#include <iostream>
#include <postgresql/libpq-fe.h>

namespace {

bool parse_price_update(const std::string &line, PriceUpdateIn &out) {
  // {"timestamp":..., "ticker":"...", "price":..., "status":"...",
  // "error":"..."}
  try {
    auto get_value = [&](const std::string &key) -> std::string {
      auto pos = line.find("\"" + key + "\"");
      if (pos == std::string::npos)
        return {};
      pos = line.find(':', pos);
      if (pos == std::string::npos)
        return {};
      ++pos;
      while (pos < line.size() && (line[pos] == ' '))
        ++pos;
      std::size_t end = pos;
      if (line[pos] == '\"') {
        ++pos;
        end = line.find('\"', pos);
        if (end == std::string::npos)
          return {};
        return line.substr(pos, end - pos);
      } else {
        end = pos;
        while (end < line.size() && line[end] != ',' && line[end] != '}') {
          ++end;
        }
        return line.substr(pos, end - pos);
      }
    };

    std::string ts_str = get_value("timestamp");
    std::string ticker = get_value("ticker");
    std::string price_str = get_value("price");
    std::string status = get_value("status");
    std::string error = get_value("error");

    if (ticker.empty())
      return false;

    out.timestamp = ts_str.empty() ? 0 : std::stoll(ts_str);
    out.ticker = ticker;
    out.price = price_str.empty() ? 0.0 : std::stod(price_str);
    out.status = status.empty() ? "ERROR" : status;
    out.error = error;
    return true;
  } catch (...) {
    return false;
  }
}

PGconn *create_pg_connection() { return nullptr; }

} // namespace

BsmService::BsmService(PricePipe<std::string> &json_pipe,
                       std::size_t num_threads, const std::string &conninfo)
    : json_pipe_(json_pipe), num_threads_(num_threads), conninfo_(conninfo) {}

BsmService::~BsmService() { stop(); }

void BsmService::start() {
  if (running_.exchange(true)) {
    return;
  }
  threads_.clear();
  threads_.reserve(num_threads_);
  for (std::size_t i = 0; i < num_threads_; ++i) {
    threads_.emplace_back(&BsmService::worker_thread, this);
  }
  dispatcher_thread_ = std::thread(&BsmService::dispatcher_thread, this);
  config_thread_ = std::thread(&BsmService::config_thread, this);
}

void BsmService::stop() {
  if (!running_.exchange(false)) {
    return;
  }
  for (auto &t : threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  threads_.clear();
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_closed_ = true;
  }
  queue_cv_.notify_all();
  if (dispatcher_thread_.joinable()) {
    dispatcher_thread_.join();
  }
  if (config_thread_.joinable()) {
    config_thread_.join();
  }
}

void BsmService::worker_thread() {
  PostgresWriter writer(conninfo_);
  if (!writer.is_connected()) {
    std::cerr
        << "PostgresWriter: connection is not ready in BsmService worker, "
        << "results will be printed to stdout\n";
  }

  while (true) {
    std::string line;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [this] { return queue_closed_ || !queue_.empty(); });
      if (queue_.empty() && queue_closed_) {
        break;
      }
      line = std::move(queue_.front());
      queue_.pop();
    }

    PriceUpdateIn in{};
    if (!parse_price_update(line, in)) {
      continue;
    }

    OptionQuote out{};
    out.timestamp = in.timestamp;
    out.ticker = in.ticker;
    out.underlying_price = in.price;

    if (in.status != "OK") {
      out.status = "ERROR";
      out.error = in.error.empty() ? "Upstream price error" : in.error;
    } else {
      BsmParams params;
      bool has_params = false;
      {
        std::lock_guard<std::mutex> lock(params_mutex_);
        auto it = params_.find(in.ticker);
        if (it != params_.end()) {
          params = it->second;
          has_params = true;
        }
      }

      if (!has_params) {
        continue;
      }

      double call_price = OptionPricer::black_scholes_call(
          in.price, params.K, params.r, params.q, params.sigma, params.T);
      out.option_price = call_price;
      out.ticker_id = params.ticker_id;
      out.conf_id = params.conf_id;
      out.status = "OK";
      out.error.clear();
    }

    if (writer.is_connected()) {
      writer.write(out);
    } else {
      std::cout << "{" << "\"timestamp\":" << out.timestamp << ","
                << "\"ticker\":\"" << out.ticker << "\","
                << "\"underlying_price\":" << out.underlying_price << ","
                << "\"option_price\":" << out.option_price << ","
                << "\"status\":\"" << out.status << "\"," << "\"error\":\""
                << out.error << "\"" << "}\n";
    }
  }
}

void BsmService::dispatcher_thread() {
  while (running_) {
    std::string line;
    if (!json_pipe_.read(line)) {
      break;
    }

    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      if (!queue_closed_) {
        queue_.push(std::move(line));
      }
    }
    queue_cv_.notify_one();
  }

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_closed_ = true;
  }
  queue_cv_.notify_all();
}

void BsmService::config_thread() {
  using namespace std::chrono_literals;

  PGconn *conn = nullptr;

  while (running_) {
    if (!conn) {
      conn = PQconnectdb(conninfo_.c_str());
      if (!conn || PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "BsmService config_thread: connection failed: "
                  << (conn ? PQerrorMessage(conn) : "PQconnectdb returned null")
                  << "\n";
        if (conn) {
          PQfinish(conn);
          conn = nullptr;
        }
        std::this_thread::sleep_for(5s);
        continue;
      }
    }

    PGresult *res =
        PQexec(conn, "SELECT t.name, p.strike, p.rate, p.dividend_yield, "
                     "p.volatility, p.maturity_years, "
                     "       p.ticker_id, p.id "
                     "FROM bsm_params p "
                     "JOIN ticker t ON t.id = p.ticker_id;");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
      std::cerr << "BsmService config_thread: query failed: "
                << PQerrorMessage(conn);
      PQclear(res);
      PQfinish(conn);
      conn = nullptr;
      std::this_thread::sleep_for(5s);
      continue;
    }

    std::unordered_map<std::string, BsmParams> new_params;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
      std::string ticker = PQgetvalue(res, i, 0);
      BsmParams p;
      p.K = std::atof(PQgetvalue(res, i, 1));
      p.r = std::atof(PQgetvalue(res, i, 2));
      p.q = std::atof(PQgetvalue(res, i, 3));
      p.sigma = std::atof(PQgetvalue(res, i, 4));
      p.T = std::atof(PQgetvalue(res, i, 5));
      p.ticker_id = std::atoll(PQgetvalue(res, i, 6));
      p.conf_id = std::atoll(PQgetvalue(res, i, 7));

      new_params[ticker] = p;
    }

    PQclear(res);

    {
      std::lock_guard<std::mutex> lock(params_mutex_);
      params_ = std::move(new_params);
    }

    std::this_thread::sleep_for(std::chrono::seconds(reload_interval_sec_));
  }

  if (conn) {
    PQfinish(conn);
  }
}
