#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright © 2021 by The qTox Project Contributors
# Copyright © 2024-2025 The TokTok team

set -euo pipefail

usage() {
  echo "$0 [--minimal|--full] --build-type [Debug|Release] [--with-gui-tests] [--sanitize] [--tidy] [--qml]"
  echo "Build script to build/test qtox from a CI environment."
  echo "--minimal or --full are required, --build-type is required."
  echo "UndefinedBehaviorSanitizer is always enabled. In Release builds, it is used without additional runtime dependencies."
  echo "--qml builds the QML user interface instead of the Qt Widgets one, and lints its QML."
}

while (($# > 0)); do
  case $1 in
    --minimal)
      MINIMAL=1
      shift
      ;;
    --full)
      MINIMAL=0
      shift
      ;;
    --sanitize)
      SANITIZE=1
      shift
      ;;
    --tidy)
      TIDY=1
      shift
      ;;
    --tidy-fix)
      TIDY_FIX=1
      shift
      ;;
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --with-gui-tests)
      GUI_TESTS=1
      shift
      ;;
    --qml)
      QML=1
      shift
      ;;
    --help | -h)
      usage
      exit 1
      ;;
    *)
      echo "Unexpected argument $1"
      usage
      exit 1
      ;;
  esac
done

if [ -z "${MINIMAL+x}" ]; then
  echo "Please build either minimal or full version of qtox"
  usage
  exit 1
fi

if [ -z "${BUILD_TYPE+x}" ]; then
  echo "Please specify build type"
  usage
  exit 1
fi

CMAKE_ARGS=()
if [ ! -z "${SANITIZE+x}" ]; then
  CMAKE_ARGS+=("-DASAN=ON")
fi

if [ ! -z "${GUI_TESTS+x}" ]; then
  CMAKE_ARGS+=("-DGUI_TESTS=ON")
fi

if [ ! -z "${QML+x}" ]; then
  CMAKE_ARGS+=("-DQTOX_UI=qml")
fi

if [ "$BUILD_TYPE" = "Debug" ]; then
  CMAKE_ARGS+=("-DREPR_RCC=ON")
fi

if [ ! -z "${TIDY+x}" ]; then
  if [ -f /usr/local/bin/clang-fake ]; then
    export CXX=clang-fake
    export CC=clang-fake
  else
    export CXX=clang++
    export CC=clang
  fi
  CMAKE_ARGS+=("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
  # Need to enable GUI tests so we can run clang-tidy on them.
  # We don't actually run them.
  CMAKE_ARGS+=("-DGUI_TESTS=ON")
fi

SRCDIR=/qtox
export CTEST_OUTPUT_ON_FAILURE=1
export QT_QPA_PLATFORM=offscreen

ccache --zero-stats

if [ "$MINIMAL" -eq 1 ]; then
  BUILD_DIR=_build-minimal
  cmake "$SRCDIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DSTRICT_OPTIONS=ON \
    -DUBSAN=ON \
    -DSMILEYS=OFF \
    -DUPDATE_CHECK=OFF \
    -DSPELL_CHECK=OFF \
    -GNinja \
    "-B$BUILD_DIR" \
    "${CMAKE_ARGS[@]}"
else
  BUILD_DIR=_build-full
  cmake "$SRCDIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DSTRICT_OPTIONS=ON \
    -DUBSAN=ON \
    -DCODE_COVERAGE=ON \
    -DUPDATE_CHECK=ON \
    -GNinja \
    "-B$BUILD_DIR" \
    "${CMAKE_ARGS[@]}"
fi

cmake --build "$BUILD_DIR"

ccache --show-stats

if [ ! -z "${TIDY+x}" ]; then
  if [ ! -z "${TIDY_FIX+x}" ]; then
    clang-tidy -list-checks
    run-clang-tidy -quiet -fix -format -p "$BUILD_DIR" \
      -exclude-header-filter '/usr/.*' \
      audio/include/ \
      audio/src/ \
      src/ \
      test/include/ \
      test/src/ \
      util/include/ \
      util/src/
  else
    tools/lsp_tidy.py --compile-commands-dir "$BUILD_DIR" --wait
  fi
else
  if [ ! -z "${QML+x}" ]; then
    cmake --build "$BUILD_DIR" --target qtox_qml_lint
  fi
  ctest -j"$(nproc)" --test-dir "$BUILD_DIR" --output-on-failure
fi
