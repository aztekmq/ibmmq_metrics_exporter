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
USERNAME="${IBMMQ_USER:-}"
PASSWORD="${IBMMQ_PASSWORD:-}"
START_PORT="${IBMMQ_EXPORTER_BASE_PORT:-19157}"
PUBLIC_PORT="${IBMMQ_EXPORTER_PUBLIC_PORT:-9157}"
FAN_IN_BIN="/usr/local/bin/ibmmq-exporter/metrics_fan_in.py"
RUNTIME_CONFIG_DIR="/tmp/ibmmq-exporter-target-configs"

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

if ! [[ "$PUBLIC_PORT" =~ ^[0-9]+$ ]]; then
  echo "ERROR: IBMMQ_EXPORTER_PUBLIC_PORT must be a number." >&2
  exit 1
fi

if [[ ! -x "$FAN_IN_BIN" ]]; then
  echo "ERROR: Fan-in metrics script not found or not executable: $FAN_IN_BIN" >&2
  exit 1
fi

pids=()
internal_ports=()

render_target_config() {
  local target_cfg="$1"
  local qmgr="$2"
  local host="$3"
  local port="$4"

  awk -v qmgr="$qmgr" -v channel="$CHANNEL" -v host="$host" -v port="$port" -v user="$USERNAME" -v pass="$PASSWORD" '
    BEGIN {
      in_mq = 0
    }
    {
      if ($0 ~ /^mq:[[:space:]]*$/) {
        in_mq = 1
        print $0
        next
      }

      if (in_mq && $0 ~ /^[^[:space:]]/) {
        in_mq = 0
      }

      if (in_mq) {
        if ($0 ~ /^[[:space:]]+queue_manager:[[:space:]]*/) {
          print "  queue_manager: \"" qmgr "\""
          next
        }
        if ($0 ~ /^[[:space:]]+channel:[[:space:]]*/) {
          print "  channel: \"" channel "\""
          next
        }
        if ($0 ~ /^[[:space:]]+host:[[:space:]]*/) {
          print "  host: \"" host "\""
          next
        }
        if ($0 ~ /^[[:space:]]+port:[[:space:]]*/) {
          print "  port: " port
          next
        }
        if ($0 ~ /^[[:space:]]+connection_name:[[:space:]]*/) {
          print "  connection_name: \"" host "(" port ")\""
          next
        }
        if ($0 ~ /^[[:space:]]+username:[[:space:]]*/) {
          print "  username: \"" user "\""
          next
        }
        if ($0 ~ /^[[:space:]]+password:[[:space:]]*/) {
          print "  password: \"" pass "\""
          next
        }
      }

      print $0
    }
  ' "$CONFIG_FILE" > "$target_cfg"
}

cleanup() {
  for pid in "${pids[@]:-}"; do
    if kill -0 "$pid" >/dev/null 2>&1; then
      kill "$pid" >/dev/null 2>&1 || true
    fi
  done
  wait || true
  rm -rf "$RUNTIME_CONFIG_DIR"
}

trap cleanup SIGINT SIGTERM

IFS=',' read -r -a target_array <<< "$TARGETS"
offset=0
rm -rf "$RUNTIME_CONFIG_DIR"
mkdir -p "$RUNTIME_CONFIG_DIR"

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
  target_cfg="$RUNTIME_CONFIG_DIR/config.${qmgr}.yaml"

  if [[ -z "$qmgr" || -z "$host" || -z "$port" ]]; then
    echo "ERROR: Invalid target '$target_trimmed'." >&2
    exit 1
  fi

  echo "Starting remote exporter for ${qmgr} on ${host}:${port} (metrics port ${prom_port})"
  render_target_config "$target_cfg" "$qmgr" "$host" "$port"

  env \
    "$EXPORTER_BIN" -c "$target_cfg" --continuous --prometheus-port "$prom_port" &

  pids+=("$!")
  internal_ports+=("$prom_port")
  offset=$((offset + 1))
done

if [[ ${#pids[@]} -eq 0 ]]; then
  echo "ERROR: No valid targets were provided in IBMMQ_TARGETS." >&2
  exit 1
fi

ports_csv="$(IFS=,; echo "${internal_ports[*]}")"
echo "Starting merged metrics endpoint on 0.0.0.0:${PUBLIC_PORT} from internal ports: ${ports_csv}"
TARGET_PORTS_CSV="$ports_csv" PUBLIC_PORT="$PUBLIC_PORT" "$FAN_IN_BIN" &
pids+=("$!")

wait -n
cleanup
exit 1
