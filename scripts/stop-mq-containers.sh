#!/usr/bin/env bash
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
    echo "docker CLI not found in PATH." >&2
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "Docker daemon is not reachable." >&2
    exit 1
fi

echo "Discovering running MQ-related containers..."

mapfile -t matches < <(
    docker ps --filter status=running --format '{{.ID}} {{.Names}} {{.Image}}' |
    grep -Ei 'ibmcom/mq|ibmmq|[[:space:]]mq|mq[0-9]|qm[0-9]' || true
)

if [ ${#matches[@]} -eq 0 ]; then
    echo "No running MQ-related containers found."
    exit 0
fi

echo "Stopping ${#matches[@]} container(s)..."
for entry in "${matches[@]}"; do
    container_id="${entry%% *}"
    echo "Stopping $container_id"
    docker stop "$container_id" >/dev/null
    echo "Stopped $container_id"
done

echo "MQ-related containers stopped."
