cmake_minimum_required(VERSION 3.16)
project(InstallerMaker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt6 ──────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ═══════════════════════════════════════════════════════════════════════════════
#  Target 1: Configurator (configuration tool, always built)
# ═══════════════════════════════════════════════════════════════════════════════

set(CONFIG_SOURCES
    src/configurator_main.cpp
    src/configurator.hpp
    src/configurator.cpp
)

add_executable(Configurator WIN32 ${CONFIG_SOURCES})
target_include_directories(Configurator PRIVATE src)
target_link_libraries(Configurator PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

if(MINGW)
    target_link_options(Configurator PRIVATE -static-libgcc -static-libstdc++)
endif()
set_target_properties(Configurator PROPERTIES WIN32_EXECUTABLE TRUE)

# ═══════════════════════════════════════════════════════════════════════════════
#  Target 2: Setup (installer, built only when PACKAGE_DIR is provided)
# ═══════════════════════════════════════════════════════════════════════════════

if(DEFINED PACKAGE_DIR)

    if(NOT EXISTS "${PACKAGE_DIR}/package.json")
        message(FATAL_ERROR "PACKAGE_DIR '${PACKAGE_DIR}' does not contain package.json")
    endif()
    message(STATUS "Package directory: ${PACKAGE_DIR}")

    # ── Generate package_content.qrc ─────────────────────────────────────
    set(QRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/package_content.qrc")

    set(QRC_CONTENT "<RCC>\n    <qresource prefix=\"/package\">\n")

    # package.json
    set(QRC_CONTENT "${QRC_CONTENT}        <file alias=\"package.json\">${PACKAGE_DIR}/package.json</file>\n")

    # files/ — recursive scan
    file(GLOB_RECURSE PACKAGE_FILES "${PACKAGE_DIR}/files/*")
    foreach(FULL_PATH ${PACKAGE_FILES})
        if(NOT IS_DIRECTORY "${FULL_PATH}")
            file(RELATIVE_PATH REL_PATH "${PACKAGE_DIR}" "${FULL_PATH}")
            string(REPLACE "\\" "/" REL_PATH "${REL_PATH}")
            set(QRC_CONTENT "${QRC_CONTENT}        <file alias=\"${REL_PATH}\">${FULL_PATH}</file>\n")
        endif()
    endforeach()

    set(QRC_CONTENT "${QRC_CONTENT}    </qresource>\n</RCC>\n")
    file(WRITE "${QRC_FILE}" "${QRC_CONTENT}")
    message(STATUS "Generated QRC: ${QRC_FILE}")

    # ── Setup sources ────────────────────────────────────────────────────
    set(SETUP_SOURCES
        src/main.cpp
        src/installwizard.hpp
        src/installwizard.cpp
        src/installerengine.hpp
        src/installerengine.cpp
        src/shortcut.hpp
        src/shortcut.cpp
        ${QRC_FILE}
    )

    add_executable(Setup WIN32 ${SETUP_SOURCES})
    target_include_directories(Setup PRIVATE src)
    target_link_libraries(Setup PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

    if(MINGW)
        target_link_options(Setup PRIVATE -static-libgcc -static-libstdc++)
    endif()
    set_target_properties(Setup PROPERTIES WIN32_EXECUTABLE TRUE)

endif()

# ═══════════════════════════════════════════════════════════════════════════════
#  Common flags
# ═══════════════════════════════════════════════════════════════════════════════
