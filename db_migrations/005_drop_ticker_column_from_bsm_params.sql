ALTER TABLE bsm_params
    DROP COLUMN IF EXISTS ticker;

ALTER TABLE public.ticker_price ALTER COLUMN ts_exchange TYPE timestamp USING ts_exchange::timestamp;
