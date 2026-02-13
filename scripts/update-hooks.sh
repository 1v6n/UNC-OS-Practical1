#!/usr/bin/env bash
set -euo pipefail

echo "Updating pre-commit hook revisions..."
pre-commit autoupdate

echo "Running hooks after update..."
pre-commit run --all-files

echo "Done. Review changes in .pre-commit-config.yaml and commit if valid."
