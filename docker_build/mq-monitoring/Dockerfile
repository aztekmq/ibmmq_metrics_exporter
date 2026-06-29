FROM icr.io/ibm-messaging/mq:latest

ARG IBMMQ_EXPORTER_BASE_VERSION=unknown
ARG IBMMQ_EXPORTER_BUILD_ID=manual
LABEL org.opencontainers.image.version="${IBMMQ_EXPORTER_BASE_VERSION}"
ENV IBMMQ_EXPORTER_BASE_VERSION="${IBMMQ_EXPORTER_BASE_VERSION}"
ENV IBMMQ_EXPORTER_BUILD_ID="${IBMMQ_EXPORTER_BUILD_ID}"

USER root

# Ensure principals referenced by MQSC auth records exist.
RUN grep -q '^monitoring:' /etc/group || echo 'monitoring:x:1500:' >> /etc/group \
 && grep -q '^app:' /etc/passwd || echo 'app:x:1500:1500:Application User:/nonexistent:/sbin/nologin' >> /etc/passwd

RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs

COPY --from=mq-remote-exporter:latest /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq-exporter
COPY --from=mq-remote-exporter:latest /usr/local/bin/ibmmq-exporter/VERSION /usr/local/bin/ibmmq-exporter/VERSION
COPY --from=mq-remote-exporter:latest /usr/local/bin/ibmmq-exporter/config.yaml /usr/local/bin/ibmmq-exporter/config.yaml
COPY scripts/ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh

# Preload MQSC for monitoring auth/channel setup during dev server initialization.
COPY docker_build/mq-monitoring/monitoring-auth.mqsc /etc/mqm/20-monitoring-auth.mqsc
RUN chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh \
 && chmod 0644 /etc/mqm/20-monitoring-auth.mqsc \
 && chown 1001:0 /etc/mqm/20-monitoring-auth.mqsc

USER 1001
