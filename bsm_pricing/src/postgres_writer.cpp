#include "postgres_writer.hpp"

#include <cstdlib>
#include <iostream>

PostgresWriter::PostgresWriter(const std::string &conninfo)
    : conn_(nullptr), conninfo_(conninfo) {
  ensure_connected();
}

PostgresWriter::~PostgresWriter() {
  if (conn_) {
    PQfinish(conn_);
    conn_ = nullptr;
  }
}

bool PostgresWriter::ensure_connected() {
  if (conn_ && PQstatus(conn_) == CONNECTION_OK) {
    return true;
  }

  if (conn_) {
    PQfinish(conn_);
    conn_ = nullptr;
  }

  if (conninfo_.empty()) {
    std::cerr << "PostgresWriter: empty conninfo, cannot connect\n";
    return false;
  }

  conn_ = PQconnectdb(conninfo_.c_str());
  if (PQstatus(conn_) != CONNECTION_OK) {
    std::cerr << "PostgresWriter: connection failed: " << PQerrorMessage(conn_);
    PQfinish(conn_);
    conn_ = nullptr;
    return false;
  }

  return true;
}

bool PostgresWriter::write(const OptionQuote &quote) {
  if (!ensure_connected()) {
    return false;
  }

  if (quote.status != "OK") {
    return true;
  }

  std::string ts_str = std::to_string(quote.timestamp);
  std::string ticker_id_str = std::to_string(quote.ticker_id);
  std::string conf_id_str = std::to_string(quote.conf_id);
  std::string calc_price_str = std::to_string(quote.option_price);

  const char *paramValues[4] = {ts_str.c_str(), ticker_id_str.c_str(),
                                conf_id_str.c_str(), calc_price_str.c_str()};

  PGresult *res =
      PQexecParams(conn_,
                   "INSERT INTO ticker_price (ts_exchange, ticker_id, conf_id, "
                   "option_price, calculated_price) "
                   "VALUES (to_timestamp($1), $2::bigint, $3::bigint, "
                   "$4::double precision, $4::double precision);",
                   4, nullptr, paramValues, nullptr, nullptr, 0);

  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    std::cerr << "PostgresWriter: insert into ticker_price failed: "
              << PQerrorMessage(conn_);
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}
