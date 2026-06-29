FROM mq-remote-exporter AS exporter_build

ARG IBMMQ_EXPORTER_GIT_SHA=unknown
ARG IBMMQ_EXPORTER_GIT_DIRTY=false

USER root

WORKDIR /src
COPY . /src

ARG MQ_REDIST_INC=/src/ibm_mq_redist_packages/10.0.0.0-IBM-MQC-Redist-LinuxX64/inc

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

FROM icr.io/ibm-messaging/mq:latest

ARG IBMMQ_EXPORTER_BASE_VERSION=unknown
LABEL org.opencontainers.image.version="${IBMMQ_EXPORTER_BASE_VERSION}"
ENV IBMMQ_EXPORTER_BASE_VERSION="${IBMMQ_EXPORTER_BASE_VERSION}"

USER root

# Ensure principals referenced by MQSC auth records exist.
RUN grep -q '^monitoring:' /etc/group || echo 'monitoring:x:1500:' >> /etc/group \
 && grep -q '^app:' /etc/passwd || echo 'app:x:1500:1500:Application User:/nonexistent:/sbin/nologin' >> /etc/passwd

RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs

COPY --from=exporter_build /tmp/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq-exporter
COPY VERSION /usr/local/bin/ibmmq-exporter/VERSION
COPY scripts/ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh
COPY docker_build/mq-embedded-exporter/config.local.yaml /usr/local/bin/ibmmq-exporter/config.yaml

# Preload MQSC for monitoring auth/channel setup during dev server initialization.
COPY docker_build/mq-monitoring/monitoring-auth.mqsc /etc/mqm/20-monitoring-auth.mqsc
RUN chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh \
 && chmod 0644 /etc/mqm/20-monitoring-auth.mqsc \
 && chown 1001:0 /etc/mqm/20-monitoring-auth.mqsc

USER 1001
