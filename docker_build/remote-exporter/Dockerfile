FROM mq-remote-exporter

USER root

WORKDIR /src
COPY . /src

# Refresh runtime scripts/config from the current workspace while reusing the
# previously built local C++ exporter binary and Python runtime.
RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs \
    && cp /src/scripts/remote_ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh \
    && cp /src/scripts/metrics_fan_in.py /usr/local/bin/ibmmq-exporter/metrics_fan_in.py \
    && cp /src/configs/config.collector.yaml /usr/local/bin/ibmmq-exporter/config.yaml \
    && chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/metrics_fan_in.py

USER 1001

ENTRYPOINT ["/usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh"]
CMD [""]
