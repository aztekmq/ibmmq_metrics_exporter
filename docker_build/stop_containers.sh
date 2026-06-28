#!/bin/bash
# =============================================================================
# Script Name : stop_containers.sh
# Description : Stop all running Docker containers on the host.
# Usage:
#   ./stop_containers.sh
#
# This script stops every container returned by `docker ps -q` and reports
# whether there were running containers to stop.
# =============================================================================

set -u

if ! command -v docker >/dev/null 2>&1; then
  echo "ERROR: docker CLI not found. Install Docker and try again."
  exit 1
fi

container_ids=$(docker ps -q)

if [[ -z "$container_ids" ]]; then
  echo "No running Docker containers found."
  exit 0
fi

echo "Stopping Docker containers:"
for id in $container_ids; do
  echo "  $id"
done

docker stop $container_ids
status=$?

if [[ $status -eq 0 ]]; then
  echo "All running Docker containers have been stopped."
else
  echo "One or more containers could not be stopped."
fi

exit $status
