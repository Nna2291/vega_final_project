#!/usr/bin/env bash

set -euo pipefail

PIPE_PATH="${PRICING_PIPE_PATH:-/tmp/pricing_pipe}"

if [[ ! -p "$PIPE_PATH" ]]; then
  echo "FIFO $PIPE_PATH does not exist or is not a named pipe" >&2
  exit 1
fi

while IFS= read -r line; do
  echo "$line"
done < "$PIPE_PATH"



