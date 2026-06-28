#!/bin/bash
# =============================================================================
# Script Name : build_mq_qmgrs.sh
# Description : Provision exactly three IBM MQ queue managers as Docker
#               containers (qm1, qm2, qm3), with Admin Web Console and REST
#               Admin endpoints exposed. Generates docker-compose.yml and
#               starts the services.
#
# Author      : rob lee
# Maintainer  : rob@aztekmq.net
# Version     : 1.0.0
# Created     : 2025-04-14
# Last Update : 2025-08-19
# License     : MIT (SPDX-License-Identifier: MIT)
#
# Standards & Conventions:
#   - Dates use ISO 8601 (YYYY-MM-DD).
#   - Shell is Bash using POSIX-compatible patterns where practical.
#   - External tools: Docker Engine + Docker Compose v2, ss(8) for port checks.
#   - Documentation language uses RFC 2119 keywords (MUST/SHOULD/MAY).
#   - Container metadata aligns with OCI/Docker Compose format 3.8.
#
# Usage:
#   ./build_mq.sh
#
# Behavior Summary:
#   1) Verifies required host ports are not already in use.
#   2) Stops/removes previous Compose stack (if present) and deletes data dir.
#   3) Creates per-QM data directories with ownership matching the MQ image.
#   4) Generates docker-compose.yml with services qm1, qm2, and qm3.
#   5) Starts containers and prints a summary table.
#
# Exit Codes:
#   0  Successful completion.
#   1  Port conflict or runtime error detected.
#
# Security Notes (Informational):
#   - Default credentials (e.g., MQ_APP_PASSWORD) are placeholders and MUST be
#     changed for non-development use.
#   - Consider setting MQ_ADMIN_PASSWORD and enabling TLS for production.
#
# Dependencies (Informational):
#   - docker (Engine & Compose v2: `docker compose` CLI)
#   - ss(8) for port conflict detection (iproute2). If unavailable, substitute
#     with `netstat -ltn` and adjust the check accordingly.
#
# Shell Quality:
#   - This script intentionally avoids `set -euo pipefail` to preserve original
#     behavior. If you adopt it, test thoroughly in your environment.
#   - Recommended linting with ShellCheck: https://www.shellcheck.net/
# =============================================================================
 
# --------- CONFIG ---------
# Fixed lab size: qm1, qm2, qm3.
NUM_QMGRS=3
# Base ports for host mappings. Each QM uses base + i (i in 1..N).
BASE_LISTENER_PORT=1414   # Maps to container port 1414 (MQ listener)
BASE_WEB_PORT=9443        # Maps to container port 9443 (Admin Web)
BASE_REST_PORT=9449       # Maps to container port 9449 (Admin REST)
# Root data directory for persistent volumes (one subdir per QM).
DATA_DIR="./data"
# Container image name and tag. Ensure the image is available locally/remotely.
IMAGE_NAME="mq-local-monitoring"
EMBEDDED_EXPORTER_IMAGE_NAME="mq-local-embedded-exporter"
# Docker build context for the custom image layer that provisions the
# monitoring/app OS identities and ships the replay-safe MQSC auth config.
DOCKERFILE_DIR="./mq-monitoring"
EMBEDDED_EXPORTER_DOCKERFILE="./mq-embedded-exporter/Dockerfile"
EMBEDDED_EXPORTER_CONTEXT="../"
# Output path for the generated Docker Compose definition.
COMPOSE_FILE="docker-compose.yml"
COMPOSE_PROJECT_NAME="mq_local_monitoring"
NETWORK_NAME="ibmmq_monitoring"
 
# --------- COLORS ---------
# ANSI color escape codes for user-friendly terminal output.
RED="\033[0;31m"
GREEN="\033[0;32m"
YELLOW="\033[0;33m"
CYAN="\033[0;36m"
NC="\033[0m" # No Color
 
# --------- Cleanup Old Environment ---------
# Stop/remove prior MQ containers for this lab before checking ports.
echo -e "${CYAN}🧹 Cleaning up old MQ containers and volumes...${NC}"

docker compose -p "$COMPOSE_PROJECT_NAME" -f "$COMPOSE_FILE" down >/dev/null 2>&1

existing_qm_ids=$(docker ps -aq --format '{{.Names}} {{.ID}}' | awk '/^qm[0-9]+ / {print $2}')
if [[ -n "$existing_qm_ids" ]]; then
  docker rm -f $existing_qm_ids >/dev/null 2>&1
fi

if [[ -d "$DATA_DIR" ]]; then
  sudo rm -rf "$DATA_DIR"
fi
 
# --------- Port Conflict Detection ---------
# For each queue manager instance i (1..3), compute external host ports and
# ensure none is currently bound. Fails fast on first conflict encountered.
echo -e "${CYAN}🔎 Checking for port conflicts...${NC}"
 
for ((i=1; i<=NUM_QMGRS; i++)); do
  LISTENER_PORT=$((BASE_LISTENER_PORT + i))
  WEB_PORT=$((BASE_WEB_PORT + i))
  REST_PORT=$((BASE_REST_PORT + i))
 
  for PORT in $LISTENER_PORT $WEB_PORT $REST_PORT; do
    # Uses ss(8) to check for an existing listening TCP socket on the host.
    # Note: Some platforms require sudo for ss; adapt as needed.
    if ss -ltn | grep -q ":$PORT "; then
      echo -e "${RED}❌ Port $PORT already in use. Cannot continue.${NC}"
      exit 1
    fi
  done

  # qm3 embeds exporter and exposes metrics on host port 9159.
  if [[ "$i" -eq 3 ]]; then
    if ss -ltn | grep -q ":9159 "; then
      echo -e "${RED}❌ Port 9159 already in use. Cannot continue.${NC}"
      exit 1
    fi
  fi
done
 
echo -e "${GREEN}✅ No port conflicts detected.${NC}"
 
# --------- Custom Image Build ---------
echo -e "${CYAN}ðŸ§± Building custom MQ image...${NC}"
docker build -t "$IMAGE_NAME" "$DOCKERFILE_DIR"
if [[ $? -ne 0 ]]; then
  echo -e "${RED}❌ Failed to build image '$IMAGE_NAME' from '$DOCKERFILE_DIR'.${NC}"
  exit 1
fi

# Build specialized image where ibmmq-exporter runs inside qm3.
echo -e "${CYAN}ðŸ§± Building embedded-exporter MQ image...${NC}"
docker build -t "$EMBEDDED_EXPORTER_IMAGE_NAME" -f "$EMBEDDED_EXPORTER_DOCKERFILE" "$EMBEDDED_EXPORTER_CONTEXT"
if [[ $? -ne 0 ]]; then
  echo -e "${RED}❌ Failed to build image '$EMBEDDED_EXPORTER_IMAGE_NAME'.${NC}"
  exit 1
fi
 
# --------- Create Data Directories ---------
# Create per-instance data subdirectories and set ownership to 1001:0 to match
# the MQ process user within the official container image.
echo -e "${CYAN}📂 Creating data directories...${NC}"
 
for ((i=1; i<=NUM_QMGRS; i++)); do
  QMGR_NAME="QM${i}"
  mkdir -p "${DATA_DIR}/${QMGR_NAME}"
  sudo chown -R 1001:0 "${DATA_DIR}/${QMGR_NAME}"
done
 
# --------- Generate docker-compose.yml ---------
# Compose file aligns with version 3.8 schema. For each QM instance, create
# a service named as the lowercase QMGR name (e.g., qm1), mapping computed
# host ports to the container's standard MQ ports. Healthcheck uses `dspmq`.
echo -e "${CYAN}🛠 Generating $COMPOSE_FILE...${NC}"
 
cat <<EOF > "$COMPOSE_FILE"
version: '3.8'
services:
EOF
 
for ((i=1; i<=NUM_QMGRS; i++)); do
  QMGR_NAME="QM${i}"
  LISTENER_PORT=$((BASE_LISTENER_PORT + i))
  WEB_PORT=$((BASE_WEB_PORT + i))
  REST_PORT=$((BASE_REST_PORT + i))
  SERVICE_IMAGE="$IMAGE_NAME"
  ENABLE_ADMIN_WEB="true"

  if [[ "$i" -eq 3 ]]; then
    SERVICE_IMAGE="$EMBEDDED_EXPORTER_IMAGE_NAME"
    ENABLE_ADMIN_WEB="false"
  fi
 
  cat <<EOF >> "$COMPOSE_FILE"
  ${QMGR_NAME,,}:
    image: $SERVICE_IMAGE
    container_name: ${QMGR_NAME,,}
    environment:
      - LICENSE=accept
      - MQ_QMGR_NAME=${QMGR_NAME}
      - MQ_APP_PASSWORD=passw0rd
      - MQ_ENABLE_METRICS=true
      - MQ_ENABLE_ADMIN_WEB=${ENABLE_ADMIN_WEB}
    ports:
      - "${LISTENER_PORT}:1414"
      - "${WEB_PORT}:9443"
      - "${REST_PORT}:9449"
EOF

  if [[ "$i" -eq 3 ]]; then
    cat <<EOF >> "$COMPOSE_FILE"
      - "9159:9159"
EOF
  fi

  cat <<EOF >> "$COMPOSE_FILE"
    ulimits:
      nofile:
        soft: 10240
        hard: 10240
    volumes:
      - ./data/${QMGR_NAME}:/mnt/mqm
    healthcheck:
      test: ["CMD", "sh", "-c", "dspmq | grep -q 'QM'"]
      interval: 30s
      timeout: 10s
      retries: 5
    restart: unless-stopped
    networks:
      - monitoring
 
EOF
done

cat <<EOF >> "$COMPOSE_FILE"

networks:
  monitoring:
    name: $NETWORK_NAME
    driver: bridge
EOF
 
# --------- Start Containers ---------
# Start all services in detached mode. On success, Compose exits 0 and the
# script proceeds to print a structured summary of the deployment.
echo -e "${CYAN}🚀 Starting up qm1, qm2, and qm3 IBM MQ containers...${NC}"
docker compose -p "$COMPOSE_PROJECT_NAME" up -d
if [[ $? -ne 0 ]]; then
  echo -e "${RED}❌ Failed to start MQ containers with docker compose.${NC}"
  exit 1
fi
 
# --------- Summary ---------
# Present a clean table of the deployed instances, including container names
# and host port mappings for listener, admin web, and admin REST endpoints.
echo -e "${GREEN}✅ All containers started successfully.${NC}"
echo ""
echo -e "${YELLOW}📄 Deployment Summary:${NC}"
 
printf "%-10s %-20s %-15s %-15s %-15s\n" "QMGR" "CONTAINER NAME" "LISTENER PORT" "WEB PORT" "REST PORT"
printf "%-10s %-20s %-15s %-15s %-15s\n" "----" "---------------" "-------------" "---------" "---------"
 
for ((i=1; i<=NUM_QMGRS; i++)); do
  QMGR_NAME="QM${i}"
  CONTAINER_NAME="${QMGR_NAME,,}"
  LISTENER_PORT=$((BASE_LISTENER_PORT + i))
  WEB_PORT=$((BASE_WEB_PORT + i))
  REST_PORT=$((BASE_REST_PORT + i))
 
  printf "%-10s %-20s %-15s %-15s %-15s\n" "$QMGR_NAME" "$CONTAINER_NAME" "$LISTENER_PORT" "$WEB_PORT" "$REST_PORT"
done

if [[ "$NUM_QMGRS" -ge 3 ]]; then
  echo ""
  echo -e "${YELLOW}Embedded exporter:${NC} qm3 runs exporter on container port 9159 and exposes http://localhost:9159/metrics"
fi
 
echo ""
echo -e "${CYAN}👉 To connect to a container: ${NC}"
echo -e "${CYAN}docker exec -it <container_name> bash${NC}"
echo ""
 
# =============================================================================
# End of file
# =============================================================================
