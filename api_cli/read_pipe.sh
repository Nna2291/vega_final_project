#!/usr/bin/env bash

# Скрипт-читалка именованного канала (FIFO): читает JSON-строки из программного
# канала, созданного сервисом `pricing_service`, и может дальше передавать их
# в логирование / БД / другую обработку.
#
# Путь к FIFO задаётся через переменную окружения PRICING_PIPE_PATH,
# по умолчанию используется /tmp/pricing_pipe
#
# Пример использования:
#   export PRICING_PIPE_PATH=/tmp/pricing_pipe
#   ./pricing_service &    # в одном терминале
#   ./read_pipe.sh         # в другом терминале

set -euo pipefail

PIPE_PATH="${PRICING_PIPE_PATH:-/tmp/pricing_pipe}"

if [[ ! -p "$PIPE_PATH" ]]; then
  echo "FIFO $PIPE_PATH does not exist or is not a named pipe" >&2
  exit 1
fi

while IFS= read -r line; do
  # Здесь можно добавить разбор JSON и запись в БД.
  # Пока просто выводим строку как есть.
  echo "$line"
done < "$PIPE_PATH"



