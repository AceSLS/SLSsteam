#!/bin/sh

BUILD_HOST="sls-build-host"

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
cd "$SCRIPT_DIR"

export TIMESTAMP="$(date "+%Y%m%d%H%M%S")"

distrobox enter "$BUILD_HOST-arch" -- ./make-release.sh --makepkg

export PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig"
distrobox enter "$BUILD_HOST-debian12" -- ./make-release.sh --make

export CROSS_HOST="AArch64"
export PKG_CONFIG_PATH="/usr/lib/aarch64-linux-gnu/pkgconfig"
distrobox enter "$BUILD_HOST-debian12" -- ./make-release.sh --make
