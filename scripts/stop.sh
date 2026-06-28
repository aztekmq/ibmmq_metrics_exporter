#!/bin/bash
# Stop the IBM MQ Metrics Exporter

echo "Stopping IBM MQ Metrics Exporter..."

# Find and kill any running exporter processes
pkill -f "ibmmq-exporter"

if [ $? -eq 0 ]; then
    echo "Exporter processes stopped."
else
    echo "No running exporter processes found."
fi

echo "Cleanup complete."
