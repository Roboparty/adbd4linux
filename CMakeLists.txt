# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.22)
project(roboparty_adbd LANGUAGES C CXX)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" _changelog_first_line LIMIT_COUNT 1)
string(REGEX MATCH "\\(([^)]+)\\)" _ "${_changelog_first_line}")
set(ADBD_VERSION "${CMAKE_MATCH_1}")
if(NOT ADBD_VERSION)
    set(ADBD_VERSION "0.0.0")
endif()

find_package(Git QUIET)
function(adbd_git_revision source_dir output_var)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" rev-parse --short=12 HEAD
            OUTPUT_VARIABLE _revision
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _result
            ERROR_QUIET)
        if(_result EQUAL 0 AND _revision)
            set(${output_var} "${_revision}" PARENT_SCOPE)
            return()
        endif()
    endif()
    set(${output_var} "unknown" PARENT_SCOPE)
endfunction()
adbd_git_revision("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/adb" ADB_GIT_REV)
adbd_git_revision("${CMAKE_CURRENT_SOURCE_DIR}" ADBD_GIT_REV)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This port targets Linux")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-tree build: cmake -S . -B build")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
include(GNUInstallDirs)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(BROTLI REQUIRED IMPORTED_TARGET libbrotlicommon libbrotlidec libbrotlienc)
pkg_check_modules(LZ4 REQUIRED IMPORTED_TARGET liblz4)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)

# Recreate only the disposable source copy; upstream submodules stay pristine.
set(PORT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/source")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS cmake/linux-port.patch)
file(REMOVE_RECURSE "${PORT_SOURCE}")
file(MAKE_DIRECTORY "${PORT_SOURCE}/thirdparty")
foreach(part thirdparty/adb thirdparty/core thirdparty/libbase thirdparty/logging)
    file(GLOB source_entries "${CMAKE_CURRENT_SOURCE_DIR}/${part}/*")
    if(NOT source_entries)
        message(FATAL_ERROR "Missing ${part}; run git submodule update --init --recursive")
    endif()
    get_filename_component(parent "${part}" DIRECTORY)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/${part}"
         DESTINATION "${PORT_SOURCE}/${parent}"
         PATTERN .git EXCLUDE PATTERN .gitignore EXCLUDE)
endforeach()
find_program(PATCH_EXECUTABLE patch REQUIRED)
execute_process(COMMAND "${PATCH_EXECUTABLE}" --batch --fuzz=0 --no-backup-if-mismatch -p1
                -i "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux-port.patch"
                WORKING_DIRECTORY "${PORT_SOURCE}" RESULT_VARIABLE patch_result)
if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "cmake/linux-port.patch does not apply to the pinned sources")
endif()
configure_file(cmake/linux_ffs_binding.h
               "${PORT_SOURCE}/thirdparty/adb/daemon/linux_ffs_binding.h" COPYONLY)
include(CTest)
if(BUILD_TESTING)
    add_executable(linux_ffs_binding_test tests/linux_ffs_binding_test.cpp cmake/linux_ffs_binding.cpp)
    add_test(NAME linux_ffs_binding COMMAND linux_ffs_binding_test)
endif()

set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/fmtlib EXCLUDE_FROM_ALL)
configure_file(cmake/platform_tools_version.h.in platform_tools_version.h @ONLY)
configure_file(cmake/version.h.in build/version.h @ONLY)
include(cmake/Targets.cmake)
target_sources(adbd PRIVATE cmake/linux_ffs_binding.cpp)
install(TARGETS adbd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Linux targets do not provide Android's app_process entry point. Install the
# small scrcpy protocol shim alongside adbd so stock host scrcpy can launch it.
find_program(FFMPEG_EXECUTABLE ffmpeg REQUIRED)
set(SCRCPY_TEST_STREAM "${CMAKE_CURRENT_BINARY_DIR}/test.h264")
add_custom_command(
    OUTPUT "${SCRCPY_TEST_STREAM}"
    COMMAND "${FFMPEG_EXECUTABLE}" -hide_banner -loglevel error -y
            -f lavfi -i testsrc=size=320x240:rate=30
            -t 1 -c:v libx264 -preset ultrafast -tune zerolatency
            -x264-params aud=1 -pix_fmt yuv420p -f h264 "${SCRCPY_TEST_STREAM}"
    COMMENT "Generating scrcpy fallback H.264 stream")
add_custom_target(scrcpy_test_stream ALL DEPENDS "${SCRCPY_TEST_STREAM}")

install(PROGRAMS usr/bin/app_process DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS usr/bin/scrcpy-display DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS usr/lib/scrcpy-compat/fake_server.py DESTINATION ${CMAKE_INSTALL_LIBDIR}/scrcpy-compat)
install(FILES "${SCRCPY_TEST_STREAM}" DESTINATION ${CMAKE_INSTALL_LIBDIR}/scrcpy-compat)

# Android command shims for the Linux desktop: screencap/screenrecord/input,
# getprop/setprop, wm, logcat, dumpsys and settings.
set(ANDROID_COMPAT_COMMANDS
    screencap screenrecord input getprop setprop wm logcat dumpsys settings)
foreach(command IN LISTS ANDROID_COMPAT_COMMANDS)
    install(PROGRAMS usr/bin/android-compat-wrapper
            DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME ${command})
endforeach()
install(FILES
    usr/lib/android-compat/common.py
    usr/lib/android-compat/x11.py
    usr/lib/android-compat/props.py
    usr/lib/android-compat/screencap.py
    usr/lib/android-compat/screenrecord.py
    usr/lib/android-compat/input.py
    usr/lib/android-compat/getprop.py
    usr/lib/android-compat/setprop.py
    usr/lib/android-compat/wm.py
    usr/lib/android-compat/logcat.py
    usr/lib/android-compat/dumpsys.py
    usr/lib/android-compat/settings.py
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/android-compat)

# The generic FunctionFS gadget manager is installed by debian/adbd.install;
# it is Debian packaging, not part of a CMake-only install.
