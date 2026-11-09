#!/bin/bash

set -e

USE_MAKE="0"
USE_MAKEPKG="0"

BUILD_DEBUG="0"
BUILD_TRACE="0"

for arg in "$@"
do
	if [ "$arg" == "-h" ] || [ "$arg" == "--help" ]; then
		echo "./$0 [OPTION...]"
		echo ""
		echo "  --make:     Use make for building"
		echo "  --makepkg:  Use makepkg for building"

		echo "  --debug:    Enable building of debug versions"
		echo "  --trace:    Enable building of trace versions"

		exit 0

	elif [ "$arg" == "--make" ]; then
		USE_MAKE="1"

	elif [ "$arg" == "--makepkg" ]; then
		USE_MAKEPKG="1"

	elif [ "$arg" == "--debug" ]; then
		BUILD_DEBUG="1"

	elif [ "$arg" == "--trace" ]; then
		BUILD_TRACE="1"

	else
		echo "Unknown argument $arg!"
		exit 1
	fi
done

if [[ "$USE_MAKE" = "0" && "$USE_MAKEPKG" = "0" ]]; then
	echo "Neither make nor makepkg is enabled! No packages would be build, exiting"
	exit 2
fi

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

ROOT_DIR="$(git rev-parse --show-toplevel)"
ARCH_PKG_DIR="$ROOT_DIR/pkg/slssteam-local"

CROSS_HOST="${CROSS_HOST:-$(uname -m)}"
TIMESTAMP="${TIMESTAMP:-$(date "+%Y%m%d%H%M%S")}"

VERSION="$(cat res/version.txt)"
BRANCH="$(git branch | grep "\*" | cut -d " " -f 2)"

RELEASE_DIR="$ROOT_DIR/releases/$BRANCH/$TIMESTAMP"

cd "$SCRIPT_DIR"

make_release()
{
	NAME="$1"
	DIR="$RELEASE_DIR/$NAME"

	if [ ! -d "$DIR" ]; then
		mkdir -p "$DIR"
	fi

	if [ "$USE_MAKE" = "1" ]; then
		make zips
		mv zips/* $DIR
		#zips are named SLSsteam VERSION
		rename "s/SLSsteam $VERSION/SLSsteam-$CROSS_HOST-Any-$NAME/" $DIR/*
	fi

	if [ "$USE_MAKEPKG" == "1" ]; then
		cd "$ARCH_PKG_DIR"
		./build.sh

		#Grab versions after building since makepkg will update them
		PKG_VER="$(grep "pkgver=.*" "PKGBUILD" | cut -d "=" -f 2)"
		PKG_REL="$(grep "pkgrel=.*" "PKGBUILD" | cut -d "=" -f 2)"
		PKG_ARCH="$(grep -o --perl-regexp "(?<=arch\=\(\").*(?=\"\))" "PKGBUILD")"

		#name for package is slssteam-pkgver-pkgrel-arch
		#We do not replace arch, for future proofing
		mv *.pkg.tar.zst "$DIR"
		rename "slssteam-git-$PKG_VER-$PKG_REL-$PKG_ARCH" "SLSsteam-$CROSS_HOST-Archlinux-$NAME" $DIR/*
	fi

	#Clean up
	mv $DIR/* "$RELEASE_DIR"
	rm -r "$DIR"

	cd "$SCRIPT_DIR"
}

if [ "$BUILD_DEBUG" = "1" ]; then
	DEBUG=1 make_release "debug"
fi
if [ "$BUILD_TRACE" = "1" ]; then
	TRACE=1 DEBUG=1 make_release "trace"
fi

make_release "release"
