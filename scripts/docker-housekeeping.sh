#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-safe}"

show_usage() {
  cat <<'EOF'
Usage: scripts/docker-housekeeping.sh [safe|aggressive|report]

Modes:
  report      Show current Docker disk usage only.
  safe        Remove unused images/containers/build cache (keeps named volumes).
  aggressive  Remove everything unused, including volumes.
EOF
}

require_tool() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "ERROR: required command not found: $cmd"
    exit 1
  fi
}

require_tool docker

case "$MODE" in
  report)
    docker system df -v
    ;;
  safe)
    echo "Before cleanup:"
    docker system df
    docker container prune -f
    docker image prune -f
    docker builder prune -f
    echo "After cleanup:"
    docker system df
    ;;
  aggressive)
    echo "WARNING: aggressive mode removes unused volumes too."
    echo "Before cleanup:"
    docker system df
    docker system prune -a --volumes -f
    echo "After cleanup:"
    docker system df
    ;;
  *)
    show_usage
    exit 1
    ;;
esac
