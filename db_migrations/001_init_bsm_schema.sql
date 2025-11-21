CREATE TABLE IF NOT EXISTS ticker (
    id      bigserial PRIMARY KEY,
    name    text        NOT NULL UNIQUE,
    info    text
);

CREATE TABLE IF NOT EXISTS bsm_params (
    id              bigserial PRIMARY KEY,
    ticker          text            NOT NULL,
    strike          double precision NOT NULL,
    rate            double precision NOT NULL,
    dividend_yield  double precision NOT NULL,
    volatility      double precision NOT NULL,
    maturity_years  double precision NOT NULL,
    updated_at      timestamptz      NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS ticker_price (
    id                bigserial PRIMARY KEY,
    ts_exchange       timestamptz    NOT NULL,
    ticker_id         bigint         NOT NULL,
    conf_id           bigint         NOT NULL,
    option_price      double precision,
    calculated_price  double precision NOT NULL,
    created_at        timestamptz    NOT NULL DEFAULT now(),

    CONSTRAINT fk_ticker_price_ticker
        FOREIGN KEY (ticker_id) REFERENCES ticker(id) ON DELETE CASCADE,
    CONSTRAINT fk_ticker_price_conf
        FOREIGN KEY (conf_id) REFERENCES bsm_params(id) ON DELETE RESTRICT
);
