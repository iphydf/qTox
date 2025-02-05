# cmake-format: off
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright © 2020 by The qTox Project Contributors
# Copyright © 2024-2025 The TokTok team
# cmake-format: on

add_library(
  util_library STATIC
  algorithm.cpp
  algorithm.h
  display.cpp
  display.h
  interface.h
  network.cpp
  network.h
  ranges.cpp
  ranges.h
  strongtype.cpp
  strongtype.h
  toxcoreerrorparser.cpp
  toxcoreerrorparser.h)

target_link_libraries(util_library PRIVATE Qt6::Core)
target_link_libraries(util_library PRIVATE Qt6::Network)
target_link_libraries(util_library PRIVATE qtox::warnings)
