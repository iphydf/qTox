#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright © 2024 The TokTok team

set -euo pipefail

readonly SCRIPT_DIR="$(dirname "$(realpath "$0")")"
. "$SCRIPT_DIR/macos_install_deps.sh"

# Install qTox dependencies that depend on Qt.
install_deps \
  build_extra_cmake_modules.sh \
  build_sonnet.sh
