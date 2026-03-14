#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/.build-tests"

mkdir -p "$BUILD_DIR"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/Logic.cpp" \
  "$ROOT_DIR/tests/logic_tests.cpp" \
  -o "$BUILD_DIR/logic_tests"

"$BUILD_DIR/logic_tests"
