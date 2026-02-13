#!/usr/bin/env bash
set -euo pipefail

wait_for_url() {
  local url="$1"
  local max_tries="${2:-30}"
  local sleep_seconds="${3:-2}"
  local i=1

  while [ "$i" -le "$max_tries" ]; do
    if curl -fsS "$url" >/dev/null 2>&1; then
      echo "OK: $url"
      return 0
    fi
    echo "Waiting for $url (attempt $i/$max_tries)..."
    sleep "$sleep_seconds"
    i=$((i + 1))
  done

  echo "ERROR: timeout waiting for $url"
  return 1
}

wait_for_url "http://localhost:8000/metrics"
wait_for_url "http://localhost:9090/-/ready"
wait_for_url "http://localhost:9115/-/healthy"
wait_for_url "http://localhost:9093/-/healthy"

echo "Checking metrics endpoint content..."
curl -fsS "http://localhost:8000/metrics" | grep -q "cpu_usage_percentage"

echo "Checking Prometheus target API is up..."
curl -fsS "http://localhost:9090/api/v1/targets" >/dev/null

echo "Smoke tests passed."
