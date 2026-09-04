#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/.build-tests"

mkdir -p "$BUILD_DIR"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/Logic.cpp" \
  "$ROOT_DIR/src/ProvisioningPolicy.cpp" \
  "$ROOT_DIR/tests/logic_tests.cpp" \
  -o "$BUILD_DIR/logic_tests"

"$BUILD_DIR/logic_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DESP32 \
  -I"$ROOT_DIR/tests/fakes" \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/fakes/LittleFS.cpp" \
  "$ROOT_DIR/src/StoreForwardQueue.cpp" \
  "$ROOT_DIR/tests/store_forward_queue_tests.cpp" \
  -o "$BUILD_DIR/store_forward_queue_tests"

"$BUILD_DIR/store_forward_queue_tests"

python3 -m unittest "$ROOT_DIR/tests/test_provision_device.py"
