#!/bin/bash
# =============================================================================
# Script Name : build_grafana.sh
# Description : Build and start a local Grafana container connected to the
#               Prometheus stack used by this lab.
#
# Usage:
#   ./build_grafana.sh [-P grafana_port] [-u prometheus_url] [-n network_name]
#                      [-U admin_user] [-W admin_password]
#
# Defaults:
#   grafana_port      3000
#   prometheus_url    http://prometheus-local-monitoring:9090
#   network_name      prometheus_local_monitoring_default
#   admin_user        admin
#   admin_password    admin
# =============================================================================

set -u

IMAGE_NAME="grafana/grafana:latest"
CONTAINER_NAME="grafana-local-monitoring"
COMPOSE_PROJECT_NAME="grafana_local_monitoring"
COMPOSE_FILE="docker-compose.grafana.yml"

GRAFANA_DIR="./grafana"
GRAFANA_DATA_DIR="./grafana-data"
PROVISIONING_DIR="$GRAFANA_DIR/provisioning"
DATASOURCES_DIR="$PROVISIONING_DIR/datasources"
DASHBOARDS_CFG_DIR="$PROVISIONING_DIR/dashboards"
DASHBOARDS_DIR="$GRAFANA_DIR/dashboards"
REPO_DASHBOARDS_DIR="../dashboards"

GRAFANA_PORT=3000
PROMETHEUS_URL="http://prometheus-local-monitoring:9090"
NETWORK_NAME="prometheus_local_monitoring_default"
ADMIN_USER="admin"
ADMIN_PASSWORD="admin"

print_usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  -P GRAFANA_PORT      Grafana UI listen port (default: $GRAFANA_PORT)
  -u PROMETHEUS_URL    Grafana datasource URL for Prometheus (default: $PROMETHEUS_URL)
  -n NETWORK_NAME      Docker network shared with Prometheus (default: $NETWORK_NAME)
  -U ADMIN_USER        Grafana admin username (default: $ADMIN_USER)
  -W ADMIN_PASSWORD    Grafana admin password (default: $ADMIN_PASSWORD)
  -?                   Show this help message
EOF
}

while getopts ":P:u:n:U:W:?" opt; do
  case "$opt" in
    P) GRAFANA_PORT="$OPTARG" ;;
    u) PROMETHEUS_URL="$OPTARG" ;;
    n) NETWORK_NAME="$OPTARG" ;;
    U) ADMIN_USER="$OPTARG" ;;
    W) ADMIN_PASSWORD="$OPTARG" ;;
    ?) print_usage ; exit 0 ;;
    *) print_usage ; exit 1 ;;
  esac
done

check_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "ERROR: Required command '$1' is not installed or not on PATH." >&2
    exit 1
  }
}

check_port_available() {
  local port="$1"
  if ss -ltn | grep -qE ":$port(\s|$)"; then
    return 1
  fi
  return 0
}

check_command docker

if ! docker compose version >/dev/null 2>&1; then
  echo "ERROR: Docker Compose support is not available. Ensure Docker with Compose v2 is installed." >&2
  exit 1
fi

if [[ ! "$GRAFANA_PORT" =~ ^[0-9]+$ ]]; then
  echo "ERROR: Invalid Grafana port '$GRAFANA_PORT'." >&2
  exit 1
fi

if ! check_port_available "$GRAFANA_PORT"; then
  echo "ERROR: Grafana port $GRAFANA_PORT is already in use by another process/container." >&2
  exit 1
fi

if ! docker network inspect "$NETWORK_NAME" >/dev/null 2>&1; then
  echo "Shared network '$NETWORK_NAME' not found. Creating it now..."
  docker network create "$NETWORK_NAME" >/dev/null
fi

if docker ps -aq --filter "name=^${CONTAINER_NAME}$" >/dev/null 2>&1; then
  old_container=$(docker ps -aq --filter "name=^${CONTAINER_NAME}$")
  if [[ -n "$old_container" ]]; then
    echo "Removing existing container $CONTAINER_NAME..."
    docker rm -f "$old_container" >/dev/null 2>&1 || true
  fi
fi

echo "Preparing Grafana provisioning files..."
mkdir -p "$DATASOURCES_DIR" "$DASHBOARDS_CFG_DIR" "$DASHBOARDS_DIR" "$GRAFANA_DATA_DIR"
chmod -R 0777 "$GRAFANA_DATA_DIR" || true

if compgen -G "$REPO_DASHBOARDS_DIR/*.json" >/dev/null 2>&1; then
  cp -f "$REPO_DASHBOARDS_DIR"/*.json "$DASHBOARDS_DIR"/
fi

cat > "$DATASOURCES_DIR/prometheus.yml" <<EOF
apiVersion: 1
datasources:
  - name: Prometheus
    type: prometheus
    access: proxy
    url: $PROMETHEUS_URL
    isDefault: true
    editable: true
EOF

cat > "$DASHBOARDS_CFG_DIR/dashboard-provider.yml" <<EOF
apiVersion: 1
providers:
  - name: IBM MQ Dashboards
    orgId: 1
    folder: IBM MQ
    type: file
    allowUiUpdates: true
    disableDeletion: false
    editable: true
    options:
      path: /var/lib/grafana/dashboards
EOF

cat > "$COMPOSE_FILE" <<EOF
services:
  grafana:
    image: $IMAGE_NAME
    container_name: $CONTAINER_NAME
    ports:
      - "127.0.0.1:$GRAFANA_PORT:3000"
    environment:
      - GF_SECURITY_ADMIN_USER=$ADMIN_USER
      - GF_SECURITY_ADMIN_PASSWORD=$ADMIN_PASSWORD
      - GF_USERS_ALLOW_SIGN_UP=false
    volumes:
      - ./grafana-data:/var/lib/grafana
      - ./grafana/provisioning:/etc/grafana/provisioning
      - ./grafana/dashboards:/var/lib/grafana/dashboards
    networks:
      - monitoring
    restart: unless-stopped

networks:
  monitoring:
    external: true
    name: $NETWORK_NAME
EOF

echo "Starting Grafana container..."
docker compose -p "$COMPOSE_PROJECT_NAME" -f "$COMPOSE_FILE" up -d

if [[ $? -ne 0 ]]; then
  echo "ERROR: Failed to start Grafana container." >&2
  exit 1
fi

echo "Grafana is ready."
echo ""
echo "Deployment Summary:"
echo "Grafana UI      : http://localhost:$GRAFANA_PORT/"
echo "Login           : $ADMIN_USER / $ADMIN_PASSWORD"
echo "Datasource URL  : $PROMETHEUS_URL"
echo "Dashboard dir   : $DASHBOARDS_DIR"
echo "Container name  : $CONTAINER_NAME"