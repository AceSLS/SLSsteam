#!/bin/bash

set -e

BUILD_PKG="1"
BUILD_DEBUG="1"
BUILD_TRACE="0"

for arg in "$@"
do
	if [ "$arg" == "-h" ] || [ "$arg" == "--help" ]; then
		echo "./$0 [OPTION...]"
		echo ""
		echo "  --no-pkg:		Don't build the Arch package"
		echo "  --no-debug:		Don't build a debug zip"
		echo "  --trace:		Build a trace zip"
		exit 0
	
	elif [ "$arg" == "--no-pkg" ]; then
		BUILD_PKG="0"

	elif [ "$arg" == "--no-debug" ]; then
		BUILD_DEBUG="0"

	elif [ "$arg" == "--trace" ]; then
		BUILD_TRACE="1"

	else
		echo "Unknown argument $arg!"
		exit 1
	fi
done

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
cd "$SCRIPT_DIR"

CROSS_HOST="${CROSS_HOST:-$(uname -m)}"
TIMESTAMP="${TIMESTAMP:-$(date "+%Y%m%d%H%M%S")}"

VERSION="$(cat res/version.txt)"

ARCH_PKG_DIR="pkg/slssteam"
PKG_VER="$(grep "pkgver=.*" "$ARCH_PKG_DIR/PKGBUILD" | cut -d "=" -f 2)"
PKG_REL="$(grep "pkgrel=.*" "$ARCH_PKG_DIR/PKGBUILD" | cut -d "=" -f 2)"

BRANCH="$(git branch | grep "\*" | cut -d " " -f 2)"
RELEASE_DIR="releases/$BRANCH/$TIMESTAMP"

make_release()
{
	NAME="$1"
	DIR="$RELEASE_DIR/$NAME"

	if [ ! -d "$DIR" ]; then
		mkdir -p "$DIR"
	fi

	make zips
	mv zips/* $DIR
	#zips are named SLSsteam VERSION
	rename "s/SLSsteam $VERSION/SLSsteam-$CROSS_HOST-Any-$NAME/" $DIR/*

	if [ "$BUILD_PKG" == "1" ]; then
		cd "$ARCH_PKG_DIR"
		makepkg -Ccf

		cd "$SCRIPT_DIR"
		mv pkg/slssteam/*.pkg.tar.zst "$DIR"
		#name for package is slssteam-pkgver-pkgrel-arch
		#We do not replace arch, for future proofing
		rename "slssteam-$PKG_VER-$PKG_REL" "SLSsteam-Arch-$NAME" $DIR/*
	fi

	mv $DIR/* "$RELEASE_DIR"
	rm -r "$DIR"
}

if [ "$BUILD_TRACE" = "1" ]; then
	TRACE=1 DEBUG=1 make_release "trace"
fi
if [ "$BUILD_DEBUG" = "1" ]; then
	DEBUG=1 make_release "debug"
fi

make_release "release"
