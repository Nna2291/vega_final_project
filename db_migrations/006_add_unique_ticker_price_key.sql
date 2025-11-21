CREATE UNIQUE INDEX IF NOT EXISTS idx_ticker_price_ts_ticker_conf_uniq
    ON ticker_price (ts_exchange, ticker_id, conf_id);
