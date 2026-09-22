#!/bin/bash

HEADER_FILE="src/version.hpp"

BRANCH="$(git branch | grep "\*" | cut -d " " -f 2)"
LAST_COMMIT_HASH="$(git rev-parse --verify HEAD)"
VERSION="$(cat "./res/version.txt")"

EMBEDED_VERSION="$(cat "$HEADER_FILE")"
NEW_EMBEDED="#pragma once

#include <cstdint>


constexpr const char* BUILD_BRANCH = \"$BRANCH\";
constexpr const char* LAST_COMMIT_HASH = \"$LAST_COMMIT_HASH\";
constexpr uint64_t VERSION = $VERSION;"

#Do not update version when nothing changed
#Otherwise the makefile will keep recompiling it when switching
#compiler flags/compilers
if [ "$EMBEDED_VERSION" != "$NEW_EMBEDED" ]; then
	echo "$NEW_EMBEDED" > "$HEADER_FILE"
fi
