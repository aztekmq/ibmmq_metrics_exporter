FROM ibmcom/mq:latest

USER root

# Ensure principals referenced by MQSC auth records exist.
RUN getent group monitoring >/dev/null || groupadd -r monitoring \
 && id -u app >/dev/null 2>&1 || useradd -r -g monitoring -M app

# Preload MQSC for monitoring auth/channel setup during dev server initialization.
COPY monitoring-auth.mqsc /etc/mqm/20-monitoring-auth.mqsc
RUN chmod 0644 /etc/mqm/20-monitoring-auth.mqsc && chown 1001:0 /etc/mqm/20-monitoring-auth.mqsc

USER 1001
