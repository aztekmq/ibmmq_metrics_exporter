#!/bin/bash
# =============================================================================
# Script Name : remote_ibmmq_exporter.sh
# Description : Start one or more ibmmq-exporter processes in a single remote
#               exporter container (one process per MQ target).
#
# Target format in IBMMQ_TARGETS:
#   QM1@qm1:1414,QM2@qm2:1414
# =============================================================================

set -euo pipefail

CONFIG_FILE="/usr/local/bin/ibmmq-exporter/config.yaml"
EXPORTER_BIN="/usr/local/bin/ibmmq-exporter/ibmmq-exporter"
TARGETS="${IBMMQ_TARGETS:-}"
CHANNEL="${IBMMQ_CHANNEL:-EXPORTER.SVRCONN}"
START_PORT="${IBMMQ_EXPORTER_BASE_PORT:-9157}"

if [[ ! -x "$EXPORTER_BIN" ]]; then
  echo "ERROR: Exporter binary not found or not executable: $EXPORTER_BIN" >&2
  exit 1
fi

if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "ERROR: Config file not found: $CONFIG_FILE" >&2
  exit 1
fi

if [[ -z "$TARGETS" ]]; then
  echo "ERROR: IBMMQ_TARGETS is required (example: QM1@qm1:1414,QM2@qm2:1414)." >&2
  exit 1
fi

if ! [[ "$START_PORT" =~ ^[0-9]+$ ]]; then
  echo "ERROR: IBMMQ_EXPORTER_BASE_PORT must be a number." >&2
  exit 1
fi

pids=()

cleanup() {
  for pid in "${pids[@]:-}"; do
    if kill -0 "$pid" >/dev/null 2>&1; then
      kill "$pid" >/dev/null 2>&1 || true
    fi
  done
  wait || true
}

trap cleanup SIGINT SIGTERM

IFS=',' read -r -a target_array <<< "$TARGETS"
offset=0

for target in "${target_array[@]}"; do
  target_trimmed="$(echo "$target" | xargs)"
  if [[ -z "$target_trimmed" ]]; then
    continue
  fi

  if [[ "$target_trimmed" != *@*:* ]]; then
    echo "ERROR: Invalid target format '$target_trimmed'. Expected QM@host:port." >&2
    exit 1
  fi

  qmgr="${target_trimmed%@*}"
  endpoint="${target_trimmed#*@}"
  host="${endpoint%:*}"
  port="${endpoint##*:}"
  prom_port=$((START_PORT + offset))

  if [[ -z "$qmgr" || -z "$host" || -z "$port" ]]; then
    echo "ERROR: Invalid target '$target_trimmed'." >&2
    exit 1
  fi

  echo "Starting remote exporter for ${qmgr} on ${host}:${port} (metrics port ${prom_port})"

  env \
    IBMMQ_QUEUE_MANAGER="$qmgr" \
    IBMMQ_CHANNEL="$CHANNEL" \
    IBMMQ_HOST="$host" \
    IBMMQ_PORT="$port" \
    IBMMQ_CONNECTION_NAME="${host}(${port})" \
    "$EXPORTER_BIN" -c "$CONFIG_FILE" --continuous --prometheus-port "$prom_port" &

  pids+=("$!")
  offset=$((offset + 1))
done

if [[ ${#pids[@]} -eq 0 ]]; then
  echo "ERROR: No valid targets were provided in IBMMQ_TARGETS." >&2
  exit 1
fi

wait -n
cleanup
exit 1
