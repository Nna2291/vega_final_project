#include "bsm_service.hpp"
#include "messages.hpp"
#include "postgres_writer.hpp"
#include "price_pipe.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace {

struct CliConfig {
  std::string pg_conninfo;
  std::string pg_host;
  std::string pg_port;
  std::string pg_user;
  std::string pg_password;
  std::string pg_db;
  std::string pipe_path{"/tmp/pricing_pipe"};
};

CliConfig parse_cli(int argc, char **argv) {
  CliConfig cfg;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next_string = [&](std::string &value) {
      if (i + 1 >= argc)
        return;
      value = argv[++i];
    };

    if (arg == "--pg-conninfo") {
      next_string(cfg.pg_conninfo);
    } else if (arg == "--pg-host") {
      next_string(cfg.pg_host);
    } else if (arg == "--pg-port") {
      next_string(cfg.pg_port);
    } else if (arg == "--pg-user") {
      next_string(cfg.pg_user);
    } else if (arg == "--pg-password") {
      next_string(cfg.pg_password);
    } else if (arg == "--pg-db" || arg == "--pg-database") {
      next_string(cfg.pg_db);
    } else if (arg == "--pipe-path") {
      next_string(cfg.pipe_path);
    }
  }
  return cfg;
}

int open_fifo_for_reading(const std::string &path) {
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    std::cerr << "Failed to open fifo for reading at " << path << ": "
              << std::strerror(errno) << "\n";
    return -1;
  }
  return fd;
}

} // namespace

int main(int argc, char **argv) {
  CliConfig cfg = parse_cli(argc, argv);
  if (cfg.pg_conninfo.empty()) {
    if (cfg.pg_host.empty() || cfg.pg_user.empty() || cfg.pg_db.empty()) {
      std::cerr << "Missing database connection parameters. "
                << "Provide either --pg-conninfo "
                << "or all of --pg-host, --pg-user, --pg-db\n";
      return 1;
    }

    std::string ci =
        "host=" + cfg.pg_host + " user=" + cfg.pg_user + " dbname=" + cfg.pg_db;
    if (!cfg.pg_port.empty()) {
      ci += " port=" + cfg.pg_port;
    }
    if (!cfg.pg_password.empty()) {
      ci += " password=" + cfg.pg_password;
    }
    cfg.pg_conninfo = std::move(ci);
  }

  int fifo_fd = open_fifo_for_reading(cfg.pipe_path);
  if (fifo_fd < 0) {
    return 1;
  }

  PricePipe<std::string> json_pipe;

  std::size_t threads = std::thread::hardware_concurrency();
  if (threads == 0) {
    threads = 4;
  }

  BsmService service(json_pipe, threads, cfg.pg_conninfo);
  service.start();

  FILE *f = fdopen(fifo_fd, "r");
  if (!f) {
    std::cerr << "fdopen failed\n";
    json_pipe.close();
    service.stop();
    ::close(fifo_fd);
    return 1;
  }

  char *lineptr = nullptr;
  size_t n = 0;
  while (true) {
    ssize_t len = getline(&lineptr, &n, f);
    if (len <= 0) {
      break;
    }
    std::string line(lineptr, static_cast<std::size_t>(len));
    json_pipe.write(line);
  }
  free(lineptr);

  fclose(f);
  json_pipe.close();
  service.stop();
  return 0;
}
