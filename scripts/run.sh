#!/usr/bin/env bash
# Run the IBM MQ Metrics Exporter
#
# Usage:
#   ./run.sh                                    # Default config
#   ./run.sh -c ../configs/default.yaml         # Specific config
#   ./run.sh --continuous --interval 10         # Continuous mode
#   ./run.sh -v                                 # Verbose

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
BINARY="${BUILD_DIR}/ibmmq-exporter"

if [[ ! -f "$BINARY" ]]; then
    echo "Binary not found at ${BINARY}. Run build.sh first."
    exit 1
fi

# Default: pass through all arguments; add default config if -c not provided
ARGS=("$@")
HAS_CONFIG=false
for arg in "$@"; do
    if [[ "$arg" == "-c" || "$arg" == "--config" ]]; then
        HAS_CONFIG=true
        break
    fi
done

if [[ "$HAS_CONFIG" == false ]]; then
    DEFAULT_CONFIG="${PROJECT_ROOT}/configs/default.yaml"
    if [[ -f "$DEFAULT_CONFIG" ]]; then
        ARGS=("-c" "$DEFAULT_CONFIG" "${ARGS[@]}")
    fi
fi

echo "Starting IBM MQ Metrics Exporter..."
echo "Binary: ${BINARY}"
echo ""

exec "$BINARY" "${ARGS[@]}"
