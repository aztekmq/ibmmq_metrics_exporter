# Multi-stage Dockerfile for IBM MQ Metrics Exporter (C++)
# Supports building with or without real IBM MQ client libraries.

# ============================================================
# Stage 1: Build
# ============================================================
FROM gcc:13-bookworm AS builder

# Install build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build git ca-certificates pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Optionally copy IBM MQ client headers/libs from host
# If not available, the build uses stub headers automatically.
ARG USE_STUB_MQ=ON
ARG MQ_HOME=/opt/mqm

WORKDIR /src

# Copy source
COPY CMakeLists.txt ./
COPY cmake/ cmake/
COPY include/ include/
COPY src/ src/
COPY configs/ configs/

# Configure
RUN cmake -S . -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=g++ \
    -DIBMMQ_EXPORTER_USE_STUB_MQ=${USE_STUB_MQ} \
    ${MQ_HOME:+-DMQ_HOME=${MQ_HOME}}

# Build
RUN cmake --build build --config Release --parallel $(nproc)

# ============================================================
# Stage 2: Runtime
# ============================================================
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN groupadd -r ibmmq && useradd -r -g ibmmq -s /sbin/nologin ibmmq

# Copy binary and config
COPY --from=builder /src/build/ibmmq-exporter /usr/local/bin/ibmmq-exporter
COPY --from=builder /src/configs/ /etc/ibmmq-exporter/

# If IBM MQ client libs exist, copy them
RUN mkdir -p /opt/mqm/lib64
# COPY --from=builder /opt/mqm/lib64/ /opt/mqm/lib64/ 2>/dev/null || true

ENV LD_LIBRARY_PATH=/opt/mqm/lib64

# Expose metrics port
EXPOSE 9091

# Health check
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:9091/metrics || exit 1

USER ibmmq
WORKDIR /home/ibmmq

ENTRYPOINT ["ibmmq-exporter"]
CMD ["-c", "/etc/ibmmq-exporter/default.yaml", "--continuous", "--log-format", "json"]
