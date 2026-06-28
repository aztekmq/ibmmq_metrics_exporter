FROM mq-local-monitoring AS builder

USER root

RUN microdnf install -y \
      gcc \
      gcc-c++ \
      make \
      cmake \
      git \
      openssl-devel \
      libcurl-devel \
      python3 \
    && microdnf clean all

WORKDIR /src
COPY . /src

RUN cmake -S /src -B /src/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /src/build --parallel "$(nproc 2>/dev/null || echo 4)"

FROM mq-local-monitoring

USER root

# Install exporter + MQ service scripts for in-container startup.
RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs

COPY --from=builder /src/build/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq-exporter
COPY --from=builder /src/scripts/ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh
COPY --from=builder /src/scripts/ibmmq_exporter.mqsc /etc/mqm/30-ibmmq-exporter-service.mqsc
COPY --from=builder /src/docker_build/mq-embedded-exporter/config.local.yaml /usr/local/bin/ibmmq-exporter/config.yaml

RUN chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq_exporter.sh \
    && chmod 0644 /etc/mqm/30-ibmmq-exporter-service.mqsc \
    && chown 1001:0 /etc/mqm/30-ibmmq-exporter-service.mqsc

USER 1001
