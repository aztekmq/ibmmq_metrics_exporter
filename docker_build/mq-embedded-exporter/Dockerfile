FROM mq-local-monitoring

ARG IBMMQ_EXPORTER_BASE_VERSION=unknown
ARG IBMMQ_EXPORTER_GIT_SHA=unknown
ARG IBMMQ_EXPORTER_GIT_DIRTY=false
LABEL org.opencontainers.image.version="${IBMMQ_EXPORTER_BASE_VERSION}"
ENV IBMMQ_EXPORTER_BASE_VERSION="${IBMMQ_EXPORTER_BASE_VERSION}"

USER root

# Add MQ service registration for in-container startup on qm3.
COPY scripts/ibmmq_exporter.mqsc /etc/mqm/30-ibmmq-exporter-service.mqsc
COPY docker_build/mq-embedded-exporter/config.local.yaml /usr/local/bin/ibmmq-exporter/config.yaml

RUN chmod 0644 /etc/mqm/30-ibmmq-exporter-service.mqsc \
    && chown 1001:0 /etc/mqm/30-ibmmq-exporter-service.mqsc

USER 1001
