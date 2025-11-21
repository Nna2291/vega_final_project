ALTER TABLE bsm_params
    ADD COLUMN IF NOT EXISTS ticker_id bigint;

DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1
        FROM pg_constraint
        WHERE conname = 'fk_bsm_params_ticker'
    ) THEN
        ALTER TABLE bsm_params
            ADD CONSTRAINT fk_bsm_params_ticker
                FOREIGN KEY (ticker_id) REFERENCES ticker(id) ON DELETE CASCADE;
    END IF;
END$$;


