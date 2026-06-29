#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

version_file="$REPO_ROOT/VERSION"
base_version="$(tr -d '[:space:]' < "$version_file")"

echo "Repo base version: $base_version"
echo ""

if [[ -x "$REPO_ROOT/build/ibmmq-exporter" ]]; then
  echo "Local build:"
  "$REPO_ROOT/build/ibmmq-exporter" version || true
  echo ""
fi

echo "Docker images:"
for image in mq-local-monitoring mq-remote-exporter mq-local-embedded-exporter; do
  if docker image inspect "$image" >/dev/null 2>&1; then
    label="$(docker image inspect -f '{{ index .Config.Labels "org.opencontainers.image.version" }}' "$image" 2>/dev/null || true)"
    image_id="$(docker image inspect -f '{{ .Id }}' "$image" | cut -c1-19)"
    printf "  %-28s label=%-12s id=%s\n" "$image" "${label:-<none>}" "$image_id"
  else
    printf "  %-28s <missing>\n" "$image"
  fi
done
echo ""

echo "Docker image exporter binaries:"
for image in mq-local-monitoring mq-remote-exporter mq-local-embedded-exporter; do
  if docker image inspect "$image" >/dev/null 2>&1; then
    echo "  $image:"
    docker run --rm --entrypoint /usr/local/bin/ibmmq-exporter/ibmmq-exporter "$image" version || true
  else
    echo "  $image: image missing"
  fi
done
echo ""

echo "Running exporter binaries:"
for container in exporter qm1 qm2 qm3; do
  if docker ps --format '{{.Names}}' | grep -qx "$container"; then
    if docker exec "$container" test -x /usr/local/bin/ibmmq-exporter/ibmmq-exporter >/dev/null 2>&1; then
      echo "  $container:"
      docker exec "$container" /usr/local/bin/ibmmq-exporter/ibmmq-exporter version || true
    else
      echo "  $container: no embedded ibmmq-exporter binary"
    fi
  else
    echo "  $container: not running"
  fi
done
