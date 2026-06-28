#!/usr/bin/env bash
# Build ibmmq-exporter in a container with MQ runtime libs and copy binary back to repo.
# This enforces non-stub mode and fails fast if real MQ linkage is unavailable.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-real-mq"
IMAGE="${MQ_BUILD_IMAGE:-mq-remote-exporter:latest}"
HEADER_DIR="${MQ_HEADER_DIR:-$REPO_ROOT/ibm_mq_redist_packages/10.0.0.0-IBM-MQC-Redist-LinuxX64/inc}"

mkdir -p "$BUILD_DIR"

echo "Using build image: $IMAGE"
echo "Output directory: $BUILD_DIR"
echo "Header directory: $HEADER_DIR"

if [[ ! -f "$HEADER_DIR/cmqc.h" ]]; then
  echo "ERROR: IBM MQ header not found: $HEADER_DIR/cmqc.h" >&2
  echo "Set MQ_HEADER_DIR to a valid IBM MQ C header directory." >&2
  exit 1
fi

# Build against real IBM MQ headers from the host while linking to container MQ libs.
docker run --rm \
  --user 0 \
  --entrypoint /bin/bash \
  -v "$REPO_ROOT":"/workspace" \
  -v "$HEADER_DIR":"/mq-headers:ro" \
  -w /workspace \
  "$IMAGE" \
  -lc '
    set -euo pipefail
    if [[ ! -f /mq-headers/cmqc.h ]]; then
      echo "ERROR: Real IBM MQ headers not found at /mq-headers/cmqc.h." >&2
      exit 1
    fi
    cmake -S /workspace -B /workspace/build-real-mq \
      -DCMAKE_BUILD_TYPE=Release \
      -DIBMMQ_EXPORTER_USE_STUB_MQ=OFF \
      -DIBMMQ_EXPORTER_ALLOW_STUB_FALLBACK=OFF \
      -DMQ_INCLUDE_PATH=/mq-headers \
      -DMQ_LIB_PATH=/opt/mqm/lib64
    cmake --build /workspace/build-real-mq --parallel "$(nproc 2>/dev/null || echo 4)"
  '

BIN="$BUILD_DIR/ibmmq-exporter"
if [[ ! -f "$BIN" ]]; then
  echo "ERROR: Expected binary not found: $BIN" >&2
  exit 1
fi

echo "Built: $BIN"
ls -lh "$BIN"

if ldd "$BIN" | grep -qiE 'libmqm|libmqm_r'; then
  echo "OK: binary links to real IBM MQ libraries"
else
  echo "ERROR: binary does not link to IBM MQ runtime libraries" >&2
  exit 1
fi
