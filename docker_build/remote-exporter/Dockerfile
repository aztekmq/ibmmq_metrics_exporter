FROM gcc:13-bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    git \
    ninja-build \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

ARG IBMMQ_EXPORTER_GIT_SHA=unknown
ARG IBMMQ_EXPORTER_GIT_DIRTY=false
ARG IBMMQ_EXPORTER_BUILD_ID=manual

WORKDIR /src

COPY CMakeLists.txt VERSION /src/
COPY cmake/ /src/cmake/
COPY include/ /src/include/
COPY src/ /src/src/
COPY ibm_mq_redist_packages/10.0.0.0-IBM-MQC-Redist-LinuxX64/inc/ /mq-redist-inc/
COPY ibm_mq_redist_packages/10.0.0.0-IBM-MQC-Redist-LinuxX64/lib64/ /mq-redist-lib64/

RUN echo "Building ibmmq-exporter artifact ${IBMMQ_EXPORTER_BUILD_ID}" \
    && test -f /mq-redist-inc/cmqc.h \
    && test -f /mq-redist-lib64/libmqm_r.so \
    && test -f /mq-redist-lib64/libmqe_r.so \
    && mkdir -p /mq-link-lib64 \
    && cp /mq-redist-lib64/libmqm_r.so /mq-redist-lib64/libmqe_r.so /mq-link-lib64/ \
    && cmake -S /src -B /src/build \
       -G Ninja \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_CXX_COMPILER=g++ \
       -DIBMMQ_EXPORTER_USE_STUB_MQ=OFF \
       -DIBMMQ_EXPORTER_ALLOW_STUB_FALLBACK=OFF \
       -DIBMMQ_EXPORTER_GIT_SHA="$IBMMQ_EXPORTER_GIT_SHA" \
       -DIBMMQ_EXPORTER_GIT_DIRTY="$IBMMQ_EXPORTER_GIT_DIRTY" \
       -DMQ_INCLUDE_PATH=/mq-redist-inc \
       -DMQ_LIB_PATH=/mq-link-lib64 \
    && cmake --build /src/build --parallel "$(nproc 2>/dev/null || echo 4)"

FROM icr.io/ibm-messaging/mq:latest

ARG IBMMQ_EXPORTER_BASE_VERSION=unknown
ARG IBMMQ_EXPORTER_BUILD_ID=manual
LABEL org.opencontainers.image.version="${IBMMQ_EXPORTER_BASE_VERSION}"
ENV IBMMQ_EXPORTER_BASE_VERSION="${IBMMQ_EXPORTER_BASE_VERSION}"
ENV IBMMQ_EXPORTER_BUILD_ID="${IBMMQ_EXPORTER_BUILD_ID}"
ENV LD_LIBRARY_PATH=/opt/mqm/lib64

USER root

RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs

COPY --from=builder /src/build/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq-exporter
COPY VERSION /usr/local/bin/ibmmq-exporter/VERSION
COPY configs/config.collector.yaml /usr/local/bin/ibmmq-exporter/config.yaml
COPY scripts/remote_ibmmq_exporter.sh scripts/metrics_fan_in.py /usr/local/bin/ibmmq-exporter/

RUN chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter \
    /usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh \
    /usr/local/bin/ibmmq-exporter/metrics_fan_in.py \
    && chown -R 1001:0 /usr/local/bin/ibmmq-exporter

USER 1001

ENTRYPOINT ["/usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh"]
CMD [""]
