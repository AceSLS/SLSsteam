#!/bin/sh

export TIMESTAMP="$(date "+%Y%m%d%H%M%S")"

export PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig"
./make-release.sh --no-debug $@

export CROSS_HOST="AArch64"
export PKG_CONFIG_PATH="/usr/lib/aarch64-linux-gnu/pkgconfig"
./make-release.sh --no-pkg --no-debug
