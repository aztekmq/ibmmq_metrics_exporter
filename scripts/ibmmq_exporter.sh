#!/bin/bash

# This is used to start the IBM MQ metrics exporter as an MQ Service.
#
# The queue manager name comes in from the service definition as the
# only command line parameter.

qMgr=$1

# Set the environment to ensure we pick up libmqm.so etc.
# Try to run it for a local qmgr; if that fails fallback to a default.
# If this is a client connection, then deal with no known qmgr of the given name.
if [ -f /opt/mqm/bin/setmqenv ]; then
    . /opt/mqm/bin/setmqenv -m "$qMgr" -k >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        . /opt/mqm/bin/setmqenv -s -k
    fi
elif [ -f /usr/mqm/bin/setmqenv ]; then
    . /usr/mqm/bin/setmqenv -m "$qMgr" -k >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        . /usr/mqm/bin/setmqenv -s -k
    fi
fi

# Configuration file location
CONFIG_FILE="/usr/local/bin/ibmmq-exporter/config.yaml"

# Build arguments
ARGS="-c ${CONFIG_FILE} --continuous"

# Override queue manager from service parameter if provided
if [ -n "$qMgr" ]; then
    export IBMMQ_QUEUE_MANAGER="$qMgr"
fi

# Start via "exec" so the pid remains the same. The queue manager can
# then check the existence of the service and use the MQ_SERVER_PID value
# to kill it on shutdown.
exec /usr/local/bin/ibmmq-exporter/ibmmq-exporter $ARGS
