#pragma once

#include "messages.hpp"

#include <postgresql/libpq-fe.h>

#include <string>

class PostgresWriter {
public:
  explicit PostgresWriter(const std::string &conninfo);
  ~PostgresWriter();

  bool is_connected() const {
    return conn_ && PQstatus(conn_) == CONNECTION_OK;
  }

  bool write(const OptionQuote &quote);

private:
  PGconn *conn_{nullptr};
  std::string conninfo_;

  bool ensure_connected();
};
