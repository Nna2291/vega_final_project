DO $$
BEGIN
  IF EXISTS (
    SELECT 1
    FROM information_schema.columns
    WHERE table_schema = 'public'
      AND table_name = 'ticker_price'
      AND column_name = 'option_price'
  ) THEN
    ALTER TABLE public.ticker_price
      RENAME COLUMN option_price TO base_price;
  END IF;
END
$$;
