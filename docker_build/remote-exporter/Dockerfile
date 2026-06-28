FROM mq-remote-exporter

USER root

WORKDIR /src
COPY . /src

# Force CMake IBM MQ discovery to use the installed MQ runtime in the base image.
ENV MQ_HOME=/opt/mqm

# Path to real IBM MQ C headers bundled in this repository.
ARG MQ_REDIST_INC=/src/ibm_mq_redist_packages/10.0.0.0-IBM-MQC-Redist-LinuxX64/inc

# Rebuild exporter from current source using toolchain already present in the
# bootstrap image, then refresh runtime scripts/config.
RUN mkdir -p /usr/local/bin/ibmmq-exporter /usr/local/bin/ibmmq-exporter/config /usr/local/bin/ibmmq-exporter/logs \
     && test -f "$MQ_REDIST_INC/cmqc.h" \
    && cmake -S /src -B /src/build \
       -DCMAKE_BUILD_TYPE=Release \
       -DIBMMQ_EXPORTER_USE_STUB_MQ=OFF \
       -DIBMMQ_EXPORTER_ALLOW_STUB_FALLBACK=OFF \
       -DMQ_INCLUDE_PATH="$MQ_REDIST_INC" \
       -DMQ_LIB_PATH=/opt/mqm/lib64 \
    && cmake --build /src/build --parallel "$(nproc 2>/dev/null || echo 4)" \
    && cp /src/build/ibmmq-exporter /usr/local/bin/ibmmq-exporter/ibmmq-exporter \
    && cp /src/scripts/remote_ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh \
    && cp /src/scripts/metrics_fan_in.py /usr/local/bin/ibmmq-exporter/metrics_fan_in.py \
    && cp /src/configs/config.collector.yaml /usr/local/bin/ibmmq-exporter/config.yaml \
    && chmod +x /usr/local/bin/ibmmq-exporter/ibmmq-exporter /usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh /usr/local/bin/ibmmq-exporter/metrics_fan_in.py

USER 1001

ENTRYPOINT ["/usr/local/bin/ibmmq-exporter/remote_ibmmq_exporter.sh"]
CMD [""]
