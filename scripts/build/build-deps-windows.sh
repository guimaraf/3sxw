#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
bash "$ROOT_DIR/scripts/build/build-deps-common.sh" windows
