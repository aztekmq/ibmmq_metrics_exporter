FROM mq-remote-exporter AS exporter_build

ARG IBMMQ_EXPORTER_BASE_VERSION=unknown
ARG IBMMQ_EXPORTER_GIT_SHA=unknown
ARG IBMMQ_EXPORTER_GIT_DIRTY=false

USER root

WORKDIR /src
COPY . /src

ARG MQ_REDIST_INC=/src/ibm_mq_redist_packages/10.0.0.0-IBM-MQC-Redist-LinuxX64/inc

# Rebuild exporter in non-stub mode for embedded qm3 usage.
RUN test -f "$MQ_REDIST_INC/cmqc.h" \
        && cmake -S /src -B /src/build \
            -DCMAKE_BUILD_TYPE=Release \
            -DIBMMQ_EXPORTER_USE_STUB_MQ=OFF \
            -DIBMMQ_EXPORTER_ALLOW_STUB_FALLBACK=OFF \
            -DIBMMQ_EXPORTER_GIT_SHA="$IBMMQ_EXPORTER_GIT_SHA" \
            -DIBMMQ_EXPORTER_GIT_DIRTY="$IBMMQ_EXPORTER_GIT_DIRTY" \
            -DMQ_INCLUDE_PATH="$MQ_REDIST_INC" \
            -DMQ_LIB_PATH=/opt/mqm/lib64 \
        && cmake --build /src/build --parallel "$(nproc 2>/dev/null || echo 4)" \
        && cp /src/build/ibmmq-exporter /tmp/ibmmq-exporter

FROM mq-local-monitoring

ARG IBMMQ_EXPORTER_BASE_VERSION=unknown
ARG IBMMQ_EXPORTER_GIT_SHA=unknown
ARG IBMMQ_EXPORTER_GIT_DIRTY=false
LABEL org.opencontainers.image.version="${IBMMQ_EXPORTER_BASE_VERSION}"
ENV IBMMQ_EXPORTER_BASE_VERSION="${IBMMQ_EXPORTER_BASE_VERSION}"

USER root

# Install exporter + MQ service scripts for in-container startup.
RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs

COPY --from=exporter_build /tmp/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq-exporter
COPY VERSION /usr/local/bin/ibmmq-exporter/VERSION
COPY scripts/ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh
COPY scripts/ibmmq_exporter.mqsc /etc/mqm/30-ibmmq-exporter-service.mqsc
COPY docker_build/mq-embedded-exporter/config.local.yaml /usr/local/bin/ibmmq-exporter/config.yaml

RUN chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh \
    && chmod 0644 /etc/mqm/30-ibmmq-exporter-service.mqsc \
    && chown 1001:0 /etc/mqm/30-ibmmq-exporter-service.mqsc

USER 1001
