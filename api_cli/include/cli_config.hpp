#pragma once

#include <string>

struct CliConfig {
  bool test_mode{false};
  std::string pg_conninfo;
  std::string pg_host;
  std::string pg_port;
  std::string pg_user;
  std::string pg_password;
  std::string pg_db;
};

CliConfig parse_cli(int argc, char **argv);
