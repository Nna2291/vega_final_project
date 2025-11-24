#pragma once

#include "market_data_provider.hpp"

#include <string>
class MoexClient : public MarketDataProvider {
public:
  MoexClient();
  ~MoexClient() override;

  PriceUpdate get_price(const std::string &ticker) override;

  static double parse_last_price_from_json(const std::string &body);

  static PriceUpdate parse_update_from_json(const std::string &body,
                                            const std::string &ticker);

protected:
  virtual std::string http_get(const std::string &url) const;

private:
  std::string build_url(const std::string &ticker) const;
};
