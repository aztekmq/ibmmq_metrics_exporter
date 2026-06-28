# FROM ibmcom/mq:latest
FROM icr.io/ibm-messaging/mq:latest

USER root

# Ensure principals referenced by MQSC auth records exist.
RUN grep -q '^monitoring:' /etc/group || echo 'monitoring:x:1500:' >> /etc/group \
 && grep -q '^app:' /etc/passwd || echo 'app:x:1500:1500:Application User:/nonexistent:/sbin/nologin' >> /etc/passwd

# Preload MQSC for monitoring auth/channel setup during dev server initialization.
COPY monitoring-auth.mqsc /etc/mqm/20-monitoring-auth.mqsc
RUN chmod 0644 /etc/mqm/20-monitoring-auth.mqsc && chown 1001:0 /etc/mqm/20-monitoring-auth.mqsc

USER 1001
