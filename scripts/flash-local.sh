#!/usr/bin/env bash
set -eu

usage() {
    cat <<EOF
Usage: $0 [options]
Options:
    --build-config=./configs/.env.M5Cardputer   The .env file to use.
    --device=/dev/ttyACM0                        The serial port.
EOF
}

BUILD_CONFIG="./configs/.env.M5Cardputer"
device=""

for arg in "$@"; do
    case $arg in
        --build-config=*) BUILD_CONFIG="${arg#*=}" ;;
        --device=*)       device="${arg#*=}" ;;
        *)                usage ;;
    esac
done

if [ -z "$device" ]; then
    usage
    exit 1
fi

source "$BUILD_CONFIG"
echo "Compiling for $NAME ($FQBN) → $device"

SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
SKETCH_DIR="/tmp/m5stick-nemo"
BUILD_DIR="/tmp/nemo-build"

rm -rf "$SKETCH_DIR"
mkdir -p "$SKETCH_DIR" "$BUILD_DIR"

for f in m5stick-nemo.ino applejuice.h localization.h WatchDogsMatrix.h; do
    ln -sf "$SRCDIR/$f" "$SKETCH_DIR/$f"
done

arduino-cli compile \
    --fqbn "$FQBN" \
    --build-property build.partitions=huge_app \
    --build-property upload.maximum_size=3145728 \
    --output-dir "$BUILD_DIR" \
    "$SKETCH_DIR"

echo "Flashing to $device..."
arduino-cli upload \
    --fqbn "$FQBN" \
    --port "$device" \
    --input-dir "$BUILD_DIR" \
    "$SKETCH_DIR"

echo "Done."
