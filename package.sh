#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-/tmp}"

WORK_DIR=$(mktemp -d)
trap "rm -rf '$WORK_DIR'" EXIT

mkdir -p "$WORK_DIR/derainbowify.koplugin"
make all -C ffi
cp "$SCRIPT_DIR/ffi/build"/*.so "$WORK_DIR/derainbowify.koplugin/"
cp "$SCRIPT_DIR/LICENSE" "$SCRIPT_DIR/README.md" "$SCRIPT_DIR/main.lua" "$SCRIPT_DIR/_meta.lua" "$WORK_DIR/derainbowify.koplugin/"

cd "$WORK_DIR"
zip -r "$OUTPUT_DIR/derainbowify.koplugin.zip" derainbowify.koplugin

ls -lh "$OUTPUT_DIR/derainbowify.koplugin.zip"
