# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
#
# Android arm64 build of the game: libgow2.so (the recompiled code, the port's own code and
# SDL_main) plus the SDK libraries it links (librexruntime.so, librexgpu-xenos.so, libSDL3.so),
# built from source in one tree and staged with libc++_shared.so for Gradle.
# scripts/build-android.ps1 runs the whole chain; see docs/ANDROID.md.

cmake_minimum_required(VERSION 3.25)
project(gow2_android LANGUAGES C CXX)

if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "Configure this project with the Android NDK's toolchain file and "
                        "-DANDROID_ABI=arm64-v8a (scripts/build-android.ps1 does).")
endif()

cmake_path(ABSOLUTE_PATH CMAKE_CURRENT_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE _here)
cmake_path(GET _here PARENT_PATH _android)
cmake_path(GET _android PARENT_PATH GOW2_ROOT)

set(GOW2_REXSDK_DIR "" CACHE PATH "rexglue-sdk source tree with the Android port")
set(GOW2_GENERATED_DIR "" CACHE PATH
    "Android copy of generated/default made by tools/prepare_android_sources.py")
set(GOW2_JNILIBS_DIR "${CMAKE_BINARY_DIR}/jniLibs" CACHE PATH
    "Where the libraries are staged for Gradle (<dir>/arm64-v8a)")
set(GOW2_RECOMP_DEBUG_INFO "line-tables-only" CACHE STRING
    "Debug info for the recompiled code: line-tables-only, full or none")
# Opt-in ThinLTO for the game library: -flto=thin plus the linker's ThinLTO cache in the build directory.
option(GOW2_ANDROID_THINLTO "Enable ThinLTO for the libgow2 game library" OFF)

if(NOT EXISTS "${GOW2_REXSDK_DIR}/CMakeLists.txt" OR
   NOT EXISTS "${GOW2_REXSDK_DIR}/include/rex/main_android.h")
    message(FATAL_ERROR "GOW2_REXSDK_DIR must be a rexglue-sdk tree with the Android port "
                        "(include/rex/main_android.h); got '${GOW2_REXSDK_DIR}'")
endif()
if(NOT EXISTS "${GOW2_GENERATED_DIR}/sources.cmake" OR NOT EXISTS "${GOW2_GENERATED_DIR}/gow2_init.h")
    message(FATAL_ERROR "GOW2_GENERATED_DIR must hold the prepared copy of the generated code "
                        "(python tools/prepare_android_sources.py); got '${GOW2_GENERATED_DIR}'")
endif()
cmake_path(ABSOLUTE_PATH GOW2_GENERATED_DIR NORMALIZE OUTPUT_VARIABLE _gen)
cmake_path(GET _gen PARENT_PATH _gen_parent)
cmake_path(GET _gen FILENAME _gen_name)
cmake_path(GET _gen_parent FILENAME _gen_parent_name)
if(_gen STREQUAL "${GOW2_ROOT}/generated/default")
    message(FATAL_ERROR "GOW2_GENERATED_DIR is the Windows build's generated/default. The Android "
                        "build needs its own copy with the ARM memory barriers.")
endif()
# src/main.cpp includes "generated/default/gow2_init.h": the copy must sit in a generated/default
# folder, and its grandparent goes on the include path in place of the repository root.
if(NOT _gen_name STREQUAL "default" OR NOT _gen_parent_name STREQUAL "generated")
    message(FATAL_ERROR "GOW2_GENERATED_DIR must end in generated/default; got '${GOW2_GENERATED_DIR}'")
endif()
cmake_path(GET _gen_parent PARENT_PATH GOW2_GENERATED_INCLUDE_ROOT)

# Same baseline as scripts/build-sdk-android.ps1, for the SDK and the game alike: LSE atomics,
# Bionic's ELF TLS, and 16 KB pages.
string(APPEND CMAKE_C_FLAGS " -march=armv8.2-a -fno-emulated-tls")
string(APPEND CMAKE_CXX_FLAGS " -march=armv8.2-a -fno-emulated-tls")
string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,-z,max-page-size=16384")
# __FILE__ and the debug info name sources relative to these roots, so the libraries do not depend on
# where the checkout, the SDK tree or the NDK live and do not carry the builder's folders. For debug
# info Clang applies the last matching map, so the SDK tree, which usually sits in out\ of the
# repository, comes last and is named rexglue-sdk wherever it is.
file(TO_CMAKE_PATH "${GOW2_REXSDK_DIR}" _sdk_prefix)
file(TO_CMAKE_PATH "${ANDROID_NDK}" _ndk_prefix)
add_compile_options(
    "-ffile-prefix-map=${GOW2_ROOT}=gow2-recomp"
    "-ffile-prefix-map=${_ndk_prefix}=android-ndk"
    "-ffile-prefix-map=${_sdk_prefix}=rexglue-sdk")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#----------------------------------------------------------------------------------------------
# SDK
#----------------------------------------------------------------------------------------------
set(REXGLUE_USE_VULKAN ON CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_TRACY OFF CACHE BOOL "" FORCE)
set(REXGLUE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${GOW2_REXSDK_DIR}" rexglue-sdk EXCLUDE_FROM_ALL)

function(gow2_collect_targets dir out_var)
    get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(subdir IN LISTS subdirs)
        gow2_collect_targets("${subdir}" sub_targets)
        list(APPEND targets ${sub_targets})
    endforeach()
    set(${out_var} "${targets}" PARENT_SCOPE)
endfunction()

# The SDK writes its binaries to <sdk>/out/<platform>, the folder scripts/build-sdk-android.ps1
# builds into. Two build trees writing the same files would keep rebuilding each other's.
function(gow2_redirect_sdk_outputs dir out_dir)
    gow2_collect_targets("${dir}" targets)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|EXECUTABLE)$")
            set_target_properties(${target} PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY "${out_dir}"
                LIBRARY_OUTPUT_DIRECTORY "${out_dir}"
                RUNTIME_OUTPUT_DIRECTORY "${out_dir}")
        endif()
    endforeach()
endfunction()
gow2_redirect_sdk_outputs("${GOW2_REXSDK_DIR}" "${CMAKE_BINARY_DIR}/sdk")

foreach(_required rexruntime rexgpu-xenos SDL3-shared)
    if(NOT TARGET ${_required})
        message(FATAL_ERROR "The SDK did not define ${_required}; is SDL3 built as a shared "
                            "library on Android (thirdparty/CMakeLists.txt)?")
    endif()
endforeach()

#----------------------------------------------------------------------------------------------
# libadrenotools (BSD 2-Clause): loads a custom Vulkan driver such as Turnip, chosen in the launch
# options (issue #17). libgow2.so links the static library; the hook libraries go in the APK and the
# app copies them next to the driver before the game starts (GpuDriver.java).
#----------------------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(adrenotools
    GIT_REPOSITORY https://github.com/bylaws/libadrenotools.git
    GIT_TAG 8fae8ce254dfc1344527e05301e43f37dea2df80
    GIT_SUBMODULES_RECURSE ON)
set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}")
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(adrenotools)
set(GOW2_ADRENOTOOLS_HOOKS hook_impl main_hook)

#----------------------------------------------------------------------------------------------
# Recompiled code
#----------------------------------------------------------------------------------------------
include("${GOW2_GENERATED_DIR}/sources.cmake")

add_library(gow2_recomp OBJECT ${GENERATED_SOURCES})
target_include_directories(gow2_recomp PRIVATE "${GOW2_ROOT}/src" "${GOW2_GENERATED_DIR}")
target_link_libraries(gow2_recomp PRIVATE rex::runtime)
rexglue_apply_target_settings(gow2_recomp)
target_precompile_headers(gow2_recomp PRIVATE "${GOW2_GENERATED_DIR}/gow2_pch.h")
if(GOW2_RECOMP_DEBUG_INFO STREQUAL "none")
    target_compile_options(gow2_recomp PRIVATE -g0)
elseif(GOW2_RECOMP_DEBUG_INFO STREQUAL "line-tables-only")
    target_compile_options(gow2_recomp PRIVATE -gline-tables-only)
endif()

#----------------------------------------------------------------------------------------------
# libgow2.so: SDL_main (the SDK's SDL entry point) and the port's own code
#----------------------------------------------------------------------------------------------
# Every source of the Windows build except the Windows-only ones: gow2_xam_input_guard.cpp and
# gow2_utf8_guard.cpp rewrite x86 code and gow2_crash_watch.cpp is SEH; gow2_android_glue.cpp
# stands in for them. A new file in src/ is built here too, so a Windows-only one has to be
# added to this list.
set(GOW2_WINDOWS_ONLY_SOURCES
    gow2_xam_input_guard.cpp
    gow2_utf8_guard.cpp
    gow2_crash_watch.cpp
)
file(GLOB GOW2_SOURCES CONFIGURE_DEPENDS "${GOW2_ROOT}/src/*.cpp")
foreach(_windows_only IN LISTS GOW2_WINDOWS_ONLY_SOURCES)
    list(REMOVE_ITEM GOW2_SOURCES "${GOW2_ROOT}/src/${_windows_only}")
endforeach()
add_library(gow2 SHARED ${GOW2_SOURCES} gow2_android_glue.cpp gow2_gpu_driver.cpp)
target_compile_definitions(gow2 PRIVATE GOW2_VERSION="0.3.2")
target_include_directories(gow2 PRIVATE
    "${GOW2_GENERATED_INCLUDE_ROOT}"
    "${GOW2_ROOT}/src"
    "${GOW2_GENERATED_DIR}"
    # rex_app.h reaches imgui.h, which an installed SDK ships in its include folder; rexruntime
    # links imgui privately, so a source-tree build has to name its headers here.
    "$<TARGET_PROPERTY:imgui,INTERFACE_INCLUDE_DIRECTORIES>"
)
# renderdoc: the header that rex/ui/vulkan/instance.h includes (gow2_gpu_driver.cpp).
target_link_libraries(gow2 PRIVATE gow2_recomp rex::runtime adrenotools renderdoc log)
target_link_options(gow2 PRIVATE -Wl,--no-undefined)
# Only SDL_main is looked up from outside (SDLActivity's dlsym); exporting the ~98,000 symbols of
# the recompiled code made the dynamic linker bind them by name at every start.
set(GOW2_EXPORTS_MAP "${CMAKE_CURRENT_SOURCE_DIR}/libgow2.map")
target_link_options(gow2 PRIVATE "-Wl,--version-script=${GOW2_EXPORTS_MAP}")
set_property(TARGET gow2 APPEND PROPERTY LINK_DEPENDS "${GOW2_EXPORTS_MAP}")
if(GOW2_ANDROID_THINLTO)
    target_compile_options(gow2 PRIVATE -flto=thin)
    target_link_options(gow2 PRIVATE -flto=thin "-Wl,--thinlto-cache-dir=${CMAKE_BINARY_DIR}/thinlto-cache")
endif()
rexglue_configure_target(gow2 GPU_PLUGINS xenos)

#----------------------------------------------------------------------------------------------
# On-device tests (scripts/test-android-device.ps1): the runtime self-test and the touch-controls
# model test; not part of the APK
#----------------------------------------------------------------------------------------------
add_executable(gow2_runtime_selftest EXCLUDE_FROM_ALL tests/runtime_selftest.cpp)
target_link_libraries(gow2_runtime_selftest PRIVATE rex::runtime)
# Pure C++ without the SDK; the static C++ library lets it run without libc++_shared.so.
add_executable(gow2_touch_model_test EXCLUDE_FROM_ALL
    tests/touch_model_test.cpp "${GOW2_ROOT}/src/gow2_touch_model.cpp")
target_include_directories(gow2_touch_model_test PRIVATE "${GOW2_ROOT}/src")
target_link_options(gow2_touch_model_test PRIVATE -static-libstdc++)

#----------------------------------------------------------------------------------------------
# Staging for Gradle
#----------------------------------------------------------------------------------------------
find_file(GOW2_LIBCXX_SHARED libc++_shared.so
    PATHS "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-android"
          "${ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/sysroot/usr/lib/aarch64-linux-android"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
if(NOT GOW2_LIBCXX_SHARED)
    message(FATAL_ERROR "libc++_shared.so not found in the NDK sysroot (${CMAKE_SYSROOT})")
endif()

set(GOW2_JNILIBS_ABI_DIR "${GOW2_JNILIBS_DIR}/${ANDROID_ABI}")
add_custom_target(gow2_jnilibs ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GOW2_JNILIBS_ABI_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3-shared>"
            "$<TARGET_FILE:rexruntime>"
            "$<TARGET_FILE:rexgpu-xenos>"
            "$<TARGET_FILE:gow2>"
            "$<TARGET_FILE:hook_impl>"
            "$<TARGET_FILE:main_hook>"
            "${GOW2_LIBCXX_SHARED}"
            "${GOW2_JNILIBS_ABI_DIR}"
    COMMENT "Staging the native libraries for Gradle in ${GOW2_JNILIBS_ABI_DIR}"
    VERBATIM)
add_dependencies(gow2_jnilibs gow2 rexruntime rexgpu-xenos SDL3-shared ${GOW2_ADRENOTOOLS_HOOKS})
