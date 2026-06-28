FROM mq-remote-exporter AS exporter_src

FROM mq-local-monitoring

USER root

# Install exporter + MQ service scripts for in-container startup.
RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs

COPY --from=exporter_src /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq-exporter
COPY scripts/ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh
COPY scripts/ibmmq_exporter.mqsc /etc/mqm/30-ibmmq-exporter-service.mqsc
COPY docker_build/mq-embedded-exporter/config.local.yaml /usr/local/bin/ibmmq-exporter/config.yaml

RUN chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh \
    && chmod 0644 /etc/mqm/30-ibmmq-exporter-service.mqsc \
    && chown 1001:0 /etc/mqm/30-ibmmq-exporter-service.mqsc

USER 1001
