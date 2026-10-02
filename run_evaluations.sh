#!/usr/bin/env bash
#
# Reproduce the full evaluation sweep.
#
# Walks every evaluation_configs/**/config.json, runs the `classify_dark` binary with
# that configuration, and writes the generated results.csv and profiler_results/
# into the corresponding configuration directory.
#
# The repository root is derived from this script's location, so the script can
# be invoked from anywhere. Build the project first (see README).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$SCRIPT_DIR"
BUILD_DIR="$BASE_DIR/build"
RESULTS_DIR="$BASE_DIR/results"
NETWORK_BIN="$BUILD_DIR/classify_dark"
EVAL_DIR="$BASE_DIR/evaluation_configs"
CONFIG_PATH="$BASE_DIR/config.json"

mkdir -p "$RESULTS_DIR"

if [ ! -x "$NETWORK_BIN" ]; then
    echo "Error: $NETWORK_BIN not found. Build the project first (see README)." >&2
    exit 1
fi

if [ ! -d "$EVAL_DIR" ]; then
    echo "Error: evaluation directory $EVAL_DIR does not exist." >&2
    exit 1
fi

find "$EVAL_DIR" -type f -name "config.json" | while read -r config_file; do
    target_dir="$(dirname "$config_file")"

    echo "Processing: $target_dir"

    # Install the evaluation configuration as the active run configuration.
    cp -f "$config_file" "$CONFIG_PATH"

    # Run from the repository root so relative paths in config.json resolve.
    ( cd "$BASE_DIR" && "$NETWORK_BIN" ) || { echo "Error: run failed for $target_dir" >&2; continue; }

    # Move the generated artifacts back into the evaluation directory.
    if [ -f "$RESULTS_DIR/results.csv" ]; then
        mv "$RESULTS_DIR/results.csv" "$target_dir/"
    fi

    if [ -d "$RESULTS_DIR/profiler_results" ]; then
        rm -rf "$target_dir/profiler_results"
        mv "$RESULTS_DIR/profiler_results" "$target_dir/"
    fi
done

echo "Evaluation cycle complete."
