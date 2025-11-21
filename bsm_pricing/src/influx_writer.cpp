#include "influx_writer.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

namespace {

std::string get_env(const char* name) {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : std::string{};
}

} // namespace

InfluxWriter::InfluxWriter() {
    std::string base_url = get_env("INFLUX_URL");
    std::string org = get_env("INFLUX_ORG");
    std::string bucket = get_env("INFLUX_BUCKET");
    token_ = get_env("INFLUX_TOKEN");
    measurement_ = get_env("INFLUX_MEASUREMENT");
    if (measurement_.empty()) {
        measurement_ = "options";
    }

    if (base_url.empty() || org.empty() || bucket.empty() || token_.empty()) {
        std::cerr << "InfluxWriter: configuration incomplete, writing disabled\n";
        configured_ = false;
        return;
    }

    if (base_url.back() == '/') {
        base_url.pop_back();
    }

    url_ = base_url + "/api/v2/write?org=" + org + "&bucket=" + bucket +
           "&precision=s";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cerr << "InfluxWriter: failed to init CURL\n";
        configured_ = false;
        return;
    }

    configured_ = true;
}

InfluxWriter::~InfluxWriter() {
    if (curl_) {
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }
    curl_global_cleanup();
}

std::string InfluxWriter::build_line_protocol(const OptionQuote& q) const {
    // measurement,tag1=...,tag2=... field1=...,field2=... timestamp
    std::ostringstream ss;
    // Используем тикер как имя измерения, чтобы удобнее было читать данные.
    ss << q.ticker;

    ss << " underlying_price=" << q.underlying_price
       << ",option_price=" << q.option_price
       << ",status=\"" << q.status << "\"";

    // экранируем кавычки в тексте ошибки
    std::string err = q.error;
    for (char& c : err) {
        if (c == '\"') {
            c = '\''; // грубая замена для простоты
        }
    }
    ss << ",error=\"" << err << "\"";

    if (q.timestamp > 0) {
        ss << " " << q.timestamp;
    }

    return ss.str();
}

bool InfluxWriter::write(const OptionQuote& quote) {
    if (!configured_ || !curl_) {
        return false;
    }

    std::string body = build_line_protocol(quote);

    struct curl_slist* headers = nullptr;
    std::string auth = "Authorization: Token " + token_;
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");
    headers = curl_slist_append(headers, auth.c_str());

    curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, body.size());

    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::cerr << "InfluxWriter: CURL error: " << curl_easy_strerror(res) << "\n";
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code / 100 != 2) {
        std::cerr << "InfluxWriter: HTTP error code " << http_code << "\n";
        return false;
    }

    return true;
}


