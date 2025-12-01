#include "ticker_loader.hpp"

#include <postgresql/libpq-fe.h>

#include <iostream>

std::vector<std::string> load_tickers_from_db(const std::string &conninfo) {
  std::vector<std::string> result;

  PGconn *conn = PQconnectdb(conninfo.c_str());
  if (!conn || PQstatus(conn) != CONNECTION_OK) {
    std::cerr << "ticker_loader: connection failed: "
              << (conn ? PQerrorMessage(conn) : "PQconnectdb returned null")
              << "\n";
    if (conn) {
      PQfinish(conn);
    }
    return result;
  }

  // Загружаем только "текущие" тикеры, для которых есть параметры в bsm_params.
  // Это позволяет автоматически синхронизировать список тикеров с теми,
  // по которым реально считаются цены.
  PGresult *res = PQexec(
      conn,
      "select t.name from ticker t "
      "join bsm_params bp on t.id = bp.ticker_id;");
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    std::cerr << "ticker_loader: query failed: " << PQerrorMessage(conn);
    PQclear(res);
    PQfinish(conn);
    return result;
  }

  int rows = PQntuples(res);
  result.reserve(rows);
  for (int i = 0; i < rows; ++i) {
    const char *val = PQgetvalue(res, i, 0);
    if (val) {
      result.emplace_back(val);
    }
  }

  PQclear(res);
  PQfinish(conn);
  return result;
}
