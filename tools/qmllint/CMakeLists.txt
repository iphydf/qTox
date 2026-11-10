# cmake-format: off
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright © 2026 The TokTok team.
# cmake-format: on

# qTox's own rules for its QML code, as a qmllint plugin for qtox_qml_lint:
# qmllint loads every plugin in its directory.
find_package(Qt6 REQUIRED COMPONENTS QmlCompiler)
qt_add_plugin(qtox_qmllint_plugin SHARED qtoxlintplugin.cpp)
target_link_libraries(qtox_qmllint_plugin PRIVATE Qt6::QmlCompiler)
set_target_properties(
  qtox_qmllint_plugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                 ${CMAKE_BINARY_DIR}/qmllint)

# qmllint isn't built with the sanitizers, and can't load their runtimes with
# the plugin.
if(ASAN
   OR TSAN
   OR UBSAN)
  target_compile_options(qtox_qmllint_plugin PRIVATE -fno-sanitize=all)
  target_link_options(qtox_qmllint_plugin PRIVATE -fno-sanitize=all)
endif()
