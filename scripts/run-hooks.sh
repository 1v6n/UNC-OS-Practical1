#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-all}"

case "$MODE" in
  all)
    echo "Running pre-commit on all files..."
    pre-commit run --all-files
    ;;
  staged)
    echo "Running pre-commit on staged files..."
    pre-commit run
    ;;
  *)
    echo "Usage: $0 [all|staged]"
    exit 1
    ;;
esac
