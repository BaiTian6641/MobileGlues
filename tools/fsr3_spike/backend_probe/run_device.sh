#!/bin/sh
# Run the production-backend probe on an assigned device. Serializes use.
set -eu
SERIAL=${1:-}
ADB=${ADB:-adb}
[ -n "$SERIAL" ] && ADB="$ADB -s $SERIAL"
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../../.." && pwd)
$ADB wait-for-device
$ADB push "$ROOT/build/android-arm64-release/libmobileglues.so" /data/local/tmp/libmobileglues.so
$ADB push "$HERE/backend_probe_arm64" /data/local/tmp/backend_probe
$ADB shell 'chmod 755 /data/local/tmp/backend_probe /data/local/tmp/libmobileglues.so'
$ADB shell 'cd /data/local/tmp && ./backend_probe /data/local/tmp/libmobileglues.so'
