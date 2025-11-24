#include "moex_client.hpp"

#include <curl/curl.h>

#include <ctime>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *buffer = static_cast<std::string *>(userdata);
  buffer->append(ptr, size * nmemb);
  return size * nmemb;
}

int find_column_index(const std::string &body, std::string_view column_name) {
  std::string_view key = "\"marketdata\"";
  auto pos = body.find(key);
  if (pos == std::string::npos) {
    throw std::runtime_error("marketdata section not found");
  }

  auto columns_pos = body.find("\"columns\"", pos);
  if (columns_pos == std::string::npos) {
    throw std::runtime_error("columns not found");
  }
  auto bracket_open = body.find('[', columns_pos);
  auto bracket_close = body.find(']', bracket_open);
  if (bracket_open == std::string::npos || bracket_close == std::string::npos) {
    throw std::runtime_error("columns array malformed");
  }
  std::string_view columns(body.data() + bracket_open + 1,
                           bracket_close - bracket_open - 1);

  int index = 0;
  std::size_t start = 0;
  while (start < columns.size()) {
    auto comma = columns.find(',', start);
    if (comma == std::string::npos) {
      comma = columns.size();
    }
    auto token = std::string_view(columns.data() + start, comma - start);
    while (!token.empty() && (token.front() == ' ' || token.front() == '\n')) {
      token.remove_prefix(1);
    }
    while (!token.empty() && (token.back() == ' ' || token.back() == '\n')) {
      token.remove_suffix(1);
    }
    if (token == column_name) {
      return index;
    }
    ++index;
    start = comma + 1;
  }

  throw std::runtime_error("column not found");
}

} // namespace

MoexClient::MoexClient() { curl_global_init(CURL_GLOBAL_DEFAULT); }

MoexClient::~MoexClient() { curl_global_cleanup(); }

PriceUpdate MoexClient::get_price(const std::string &ticker) {
  const auto url = build_url(ticker);
  const auto body = http_get(url);
  return parse_update_from_json(body, ticker);
}

std::string MoexClient::build_url(const std::string &ticker) const {
  std::string lower_ticker = ticker;
  for (auto &c : lower_ticker) {
    c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
  }
  return "https://iss.moex.com/iss/engines/stock/markets/shares/boards/tqbr/"
         "securities/" +
         lower_ticker + ".json?iss.meta=off";
}

std::string MoexClient::http_get(const std::string &url) const {
  CURL *curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to init CURL");
  }

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    curl_easy_cleanup(curl);
    throw std::runtime_error("CURL request failed: " +
                             std::string(curl_easy_strerror(res)));
  }

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (http_code != 200) {
    throw std::runtime_error("HTTP error: " + std::to_string(http_code));
  }

  return response;
}

PriceUpdate MoexClient::parse_update_from_json(const std::string &body,
                                               const std::string &ticker) {
  const int last_index = find_column_index(body, "\"LAST\"");
  const int systime_index = find_column_index(body, "\"SYSTIME\"");

  std::string_view key = "\"marketdata\"";
  auto pos = body.find(key);
  if (pos == std::string::npos) {
    throw std::runtime_error("marketdata section not found");
  }

  auto data_pos = body.find("\"data\"", pos);
  if (data_pos == std::string::npos) {
    throw std::runtime_error("data not found");
  }

  auto outer_open = body.find('[', data_pos);
  if (outer_open == std::string::npos) {
    throw std::runtime_error("data array malformed");
  }

  auto inner_open = body.find('[', outer_open + 1);
  auto inner_close = body.find(']', inner_open + 1);
  if (inner_open == std::string::npos || inner_close == std::string::npos) {
    throw std::runtime_error("data row malformed");
  }

  std::string_view row(body.data() + inner_open + 1,
                       inner_close - inner_open - 1);

  int index = 0;
  std::size_t start = 0;
  std::string_view last_token;
  std::string_view systime_token;
  while (start < row.size()) {
    auto comma = row.find(',', start);
    if (comma == std::string::npos) {
      comma = row.size();
    }
    auto token = std::string_view(row.data() + start, comma - start);
    while (!token.empty() && (token.front() == ' ' || token.front() == '\n')) {
      token.remove_prefix(1);
    }
    while (!token.empty() && (token.back() == ' ' || token.back() == '\n')) {
      token.remove_suffix(1);
    }

    if (index == last_index) {
      last_token = token;
    } else if (index == systime_index) {
      systime_token = token;
    }

    ++index;
    start = comma + 1;
  }

  if (last_token.empty()) {
    throw std::runtime_error("LAST price value not found");
  }
  if (last_token == "null") {
    throw std::runtime_error("LAST price is null");
  }

  double price = std::stod(std::string(last_token));

  if (systime_token.empty() || systime_token == "null") {
    throw std::runtime_error("SYSTIME is missing");
  }
  std::string systime_str(systime_token);
  if (!systime_str.empty() && systime_str.front() == '"' &&
      systime_str.back() == '"') {
    systime_str = systime_str.substr(1, systime_str.size() - 2);
  }
  if (systime_str.size() != 19) {
    throw std::runtime_error("SYSTIME has unexpected format");
  }

  std::tm tm{};
  tm.tm_year = std::stoi(systime_str.substr(0, 4)) - 1900;
  tm.tm_mon = std::stoi(systime_str.substr(5, 2)) - 1;
  tm.tm_mday = std::stoi(systime_str.substr(8, 2));
  tm.tm_hour = std::stoi(systime_str.substr(11, 2));
  tm.tm_min = std::stoi(systime_str.substr(14, 2));
  tm.tm_sec = std::stoi(systime_str.substr(17, 2));
  std::time_t epoch = timegm(&tm);
  if (epoch == static_cast<std::time_t>(-1)) {
    throw std::runtime_error("Failed to convert SYSTIME to epoch");
  }

  std::int64_t ts = static_cast<std::int64_t>(epoch);

  PriceUpdate update;
  update.timestamp = ts;
  update.ticker = ticker;
  update.price = price;
  update.status = "OK";
  update.error.clear();
  return update;
}

double MoexClient::parse_last_price_from_json(const std::string &body) {
  auto update = parse_update_from_json(body, "DUMMY");
  return update.price;
}
