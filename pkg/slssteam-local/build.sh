#!/bin/bash

set -e

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
ROOT_DIR="$(git rev-parse --show-toplevel)"

GIT_TAR="$SCRIPT_DIR/SLSsteam.tar"

cd "$ROOT_DIR"
git ls-files -z | xargs -0 tar -cvf "$GIT_TAR"

cd "$SCRIPT_DIR"
cp -v "$ROOT_DIR/pkg/slssteam-git/PKGBUILD" "PKGBUILD"
sed -i "s|source=.*|source=(\"$GIT_TAR\")|" "PKGBUILD"
sed -i 's|cd "$srcdir/SLSsteam"|cd "$srcdir"|' "PKGBUILD"

makepkg -sfr
