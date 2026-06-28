#!/bin/bash
# =============================================================================
# Script Name : build_remote_exporter.sh
# Description : Build a Docker image for a remote MQ exporter container.
#               Uses the same base image as build_mq.sh, but does not create
#               any queue manager.
#
# The image produced is intended to host ibmmq-exporter as a service. The
# exporter binary is built from source first, then the Docker image is built
# on top of the mq-local-monitoring base image.
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BASE_IMAGE="mq-local-monitoring"
REMOTE_IMAGE="mq-remote-exporter"
BASE_IMAGE_CONTEXT="$SCRIPT_DIR/mq-monitoring"
BUILD_SCRIPT="$REPO_ROOT/scripts/build.sh"
DOCKERFILE="$SCRIPT_DIR/remote-exporter/Dockerfile"
COMPOSE_FILE="$SCRIPT_DIR/docker-compose.remote-exporter.yml"
NETWORK_NAME="ibmmq_monitoring"
BASE_METRICS_PORT=9157
BASE_LISTENER_PORT=1414
TARGET_COUNT="${1:-1}"

check_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "ERROR: Required command '$1' is not installed or not on PATH." >&2
    exit 1
  }
}

check_command docker
check_command bash

if [[ ! "$TARGET_COUNT" =~ ^[0-9]+$ ]] || [[ "$TARGET_COUNT" -lt 1 ]]; then
  echo "ERROR: Usage: $0 <number_of_qmgr_targets>" >&2
  exit 1
fi

ensure_base_image() {
  if ! docker image inspect "$BASE_IMAGE" >/dev/null 2>&1; then
    echo "Base image '$BASE_IMAGE' not found. Building it now..."
    docker build -t "$BASE_IMAGE" "$BASE_IMAGE_CONTEXT"
  else
    echo "Base image '$BASE_IMAGE' already exists."
  fi
}

build_exporter_binary() {
  echo "Exporter binary will be built inside the Docker image build stage."
}

build_image() {
  echo "Building remote exporter Docker image '$REMOTE_IMAGE'..."
  docker build -t "$REMOTE_IMAGE" -f "$DOCKERFILE" "$REPO_ROOT"
}

ensure_network() {
  if ! docker network inspect "$NETWORK_NAME" >/dev/null 2>&1; then
    echo "Creating shared monitoring network '$NETWORK_NAME'..."
    docker network create "$NETWORK_NAME" >/dev/null
  fi
}

cleanup_existing_exporters() {
  echo "Stopping/removing existing remote exporter containers..."

  if [[ -f "$COMPOSE_FILE" ]]; then
    docker compose -f "$COMPOSE_FILE" down --remove-orphans >/dev/null 2>&1
  fi

  existing_exporter_ids=$(docker ps -aq --format '{{.Names}} {{.ID}}' | awk '/^exporter([0-9]+)? / {print $2}')
  if [[ -n "$existing_exporter_ids" ]]; then
    docker rm -f $existing_exporter_ids >/dev/null 2>&1
  fi
}

generate_compose_file() {
  echo "Generating remote exporter compose file..."
  targets=""
  ports_block=""

  for ((i=1; i<=TARGET_COUNT; i++)); do
    qmgr_name="QM${i}"
    host_name="qm${i}"
    metrics_port=$((BASE_METRICS_PORT + i - 1))

    if [[ -n "$targets" ]]; then
      targets+=","
    fi
    targets+="${qmgr_name}@${host_name}:${BASE_LISTENER_PORT}"
    ports_block+="      - \"${metrics_port}:${metrics_port}\"\n"
  done

  cat > "$COMPOSE_FILE" <<EOF
version: '3.8'
services:
  exporter:
    image: $REMOTE_IMAGE
    container_name: exporter
    environment:
      - IBMMQ_TARGETS=${targets}
      - IBMMQ_CHANNEL=EXPORTER.SVRCONN
      - IBMMQ_EXPORTER_BASE_PORT=${BASE_METRICS_PORT}
      - IBMMQ_USER=
      - IBMMQ_PASSWORD=
    ports:
$(printf "%b" "$ports_block")
    networks:
      - monitoring
    restart: unless-stopped
networks:
  monitoring:
    external: true
    name: $NETWORK_NAME
EOF
}

start_exporters() {
  echo "Starting remote exporter services..."
  docker compose -f "$COMPOSE_FILE" up -d
}

main() {
  ensure_base_image
  build_exporter_binary
  build_image
  ensure_network
  cleanup_existing_exporters
  generate_compose_file
  start_exporters
  echo ""
  echo "Built image: $REMOTE_IMAGE"
  echo "Compose file: $COMPOSE_FILE"
  echo "Metrics ports: ${BASE_METRICS_PORT}..$((BASE_METRICS_PORT + TARGET_COUNT - 1))"
}

main "$@"
