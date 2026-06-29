#!/bin/bash
# =============================================================================
# Script Name : build_remote_exporter.sh
# Description : Build a Docker image for a remote MQ exporter container.
#               Uses the same base image as build_mq.sh, but does not create
#               any queue manager.
#
# The image produced is intended to host ibmmq-exporter as a service. The
# runtime reuses an existing local C++ exporter image as a bootstrap layer,
# then refreshes scripts/config from the current workspace.
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BASE_IMAGE="mq-local-monitoring"
REMOTE_IMAGE="mq-remote-exporter"
BASE_IMAGE_CONTEXT="$SCRIPT_DIR/mq-monitoring"
DOCKERFILE="$SCRIPT_DIR/remote-exporter/Dockerfile"
COMPOSE_FILE="$SCRIPT_DIR/docker-compose.remote-exporter.yml"
NETWORK_NAME="ibmmq_monitoring"
BASE_METRICS_PORT=9157
BASE_LISTENER_PORT=1414
TARGET_COUNT=2
VERSION_FILE="$REPO_ROOT/VERSION"
EXPORTER_BASE_VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"
EXPORTER_GIT_SHA="$(git -C "$REPO_ROOT" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
if [[ -n "$(git -C "$REPO_ROOT" status --porcelain 2>/dev/null || true)" ]]; then
  EXPORTER_GIT_DIRTY="true"
else
  EXPORTER_GIT_DIRTY="false"
fi

check_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "ERROR: Required command '$1' is not installed or not on PATH." >&2
    exit 1
  }
}

check_command docker
check_command bash

if [[ "$#" -gt 1 ]]; then
  echo "ERROR: Usage: $0 [2]" >&2
  exit 1
fi

if [[ "$#" -eq 1 ]] && [[ "$1" != "2" ]]; then
  echo "ERROR: This lab is fixed to qm1 and qm2 only; use '$0' (or '$0 2')." >&2
  exit 1
fi

ensure_base_image() {
  current_base_version=""
  if docker image inspect "$BASE_IMAGE" >/dev/null 2>&1; then
    current_base_version="$(docker image inspect -f '{{ index .Config.Labels "org.opencontainers.image.version" }}' "$BASE_IMAGE" 2>/dev/null || true)"
  fi

  if [[ "$current_base_version" != "$EXPORTER_BASE_VERSION" ]]; then
    echo "Building base image '$BASE_IMAGE' for version $EXPORTER_BASE_VERSION..."
    docker build \
      --build-arg "IBMMQ_EXPORTER_BASE_VERSION=$EXPORTER_BASE_VERSION" \
      --build-arg "IBMMQ_EXPORTER_GIT_SHA=$EXPORTER_GIT_SHA" \
      --build-arg "IBMMQ_EXPORTER_GIT_DIRTY=$EXPORTER_GIT_DIRTY" \
      -t "$BASE_IMAGE:latest" \
      -t "$BASE_IMAGE:$EXPORTER_BASE_VERSION" \
      "$BASE_IMAGE_CONTEXT"
  else
    echo "Base image '$BASE_IMAGE' already matches version $EXPORTER_BASE_VERSION."
  fi
  if ! docker image inspect "$REMOTE_IMAGE" >/dev/null 2>&1; then
    echo "ERROR: Bootstrap image '$REMOTE_IMAGE' is required but not found." >&2
    echo "Run './build_remote_exporter.sh' at least once on a host where the image already exists," >&2
    echo "or restore it from a local cache/registry backup before rebuilding on this host." >&2
    return 1
  fi
}

build_exporter_binary() {
  echo "Reusing previously built local C++ exporter runtime from image: $REMOTE_IMAGE"
}

build_image() {
  echo "Building remote exporter Docker image '$REMOTE_IMAGE' for base version $EXPORTER_BASE_VERSION..."
  docker build \
    --build-arg "IBMMQ_EXPORTER_BASE_VERSION=$EXPORTER_BASE_VERSION" \
    --build-arg "IBMMQ_EXPORTER_GIT_SHA=$EXPORTER_GIT_SHA" \
    --build-arg "IBMMQ_EXPORTER_GIT_DIRTY=$EXPORTER_GIT_DIRTY" \
    -t "$REMOTE_IMAGE:latest" \
    -t "$REMOTE_IMAGE:$EXPORTER_BASE_VERSION" \
    -f "$DOCKERFILE" \
    "$REPO_ROOT"
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
  exporter_user="${IBMMQ_USER:-}"
  exporter_password="${IBMMQ_PASSWORD:-}"

  for ((i=1; i<=TARGET_COUNT; i++)); do
    qmgr_name="QM${i}"
    host_name="qm${i}"

    if [[ -n "$targets" ]]; then
      targets+=","
    fi
    targets+="${qmgr_name}@${host_name}:${BASE_LISTENER_PORT}"
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
      - IBMMQ_USER=${exporter_user}
      - IBMMQ_PASSWORD=${exporter_password}
    ports:
      - "9157:9157"
      - "9158:9158"
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
  echo "Built image: $REMOTE_IMAGE:latest and $REMOTE_IMAGE:$EXPORTER_BASE_VERSION"
  echo "Compose file: $COMPOSE_FILE"
  echo "Metrics endpoints: 9157 and 9158 (one exporter per target)"
}

main "$@"
