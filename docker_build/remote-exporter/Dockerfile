FROM mq-local-monitoring

USER root

# Build dependencies for ibmmq-exporter (built against the same base image ABI)
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

# Build with real MQ libraries available in the MQ base image.
RUN cmake -S /src -B /src/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /src/build --parallel "$(nproc 2>/dev/null || echo 4)"

# Ensure exporter directories exist and install runtime files.
RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs \
    && cp /src/build/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq-exporter \
    && cp /src/scripts/remote_ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh \
    && cp /src/configs/config.collector.yaml /usr/local/bin/ibmmq-exporter/config.yaml \
    && chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh

USER 1001

ENTRYPOINT ["/usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh"]
CMD [""]
