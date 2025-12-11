insert into ticker(name) values ('SBER');

INSERT INTO public.bsm_params
(strike, rate, dividend_yield, volatility, maturity_years, ticker_id)
VALUES(1.0, 1.0, 1.0, 1.0, 1.0, (select id from ticker limit 1));