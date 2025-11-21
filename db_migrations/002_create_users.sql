DO $$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'bsm_read') THEN
        CREATE ROLE bsm_read LOGIN PASSWORD 'bsm_read_password';
    END IF;
END$$;

DO $$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'bsm_write_prices') THEN
        CREATE ROLE bsm_write_prices LOGIN PASSWORD 'bsm_write_prices_password';
    END IF;
END$$;

DO $$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'bsm_write_params') THEN
        CREATE ROLE bsm_write_params LOGIN PASSWORD 'bsm_write_params_password';
    END IF;
END$$;

GRANT USAGE ON SCHEMA public TO bsm_read, bsm_write_prices, bsm_write_params;

GRANT SELECT ON TABLE ticker, bsm_params, ticker_price TO bsm_read;

GRANT SELECT, INSERT, UPDATE, DELETE ON TABLE ticker_price TO bsm_write_prices;

GRANT SELECT ON TABLE ticker, bsm_params, ticker_price TO bsm_write_prices;

GRANT USAGE, SELECT ON SEQUENCE ticker_price_id_seq TO bsm_write_prices;

GRANT SELECT, INSERT, UPDATE, DELETE ON TABLE bsm_params TO bsm_write_params;
