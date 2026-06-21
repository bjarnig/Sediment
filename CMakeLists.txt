####################################################################################################
# Sediment - a SuperCollider granular texture-synthesizer UGen.
# Original DSP inspired by Mutable Instruments "Sediment" (Emilie Gillet), reimplemented from
# scratch. Built with SC's cmake_gen helpers (vendored in cmake_modules/).
####################################################################################################

cmake_minimum_required(VERSION 3.12)
set(project_name "Sediment")

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(SuperColliderServerPlugin RESULT_VARIABLE server_plugin_found)
if(NOT server_plugin_found)
    message(FATAL_ERROR "Could not find SuperColliderServerPlugin module in cmake_modules/")
endif()
include(SuperColliderCompilerConfig RESULT_VARIABLE compiler_config_found)
if(NOT compiler_config_found)
    message(FATAL_ERROR "Could not find SuperColliderCompilerConfig module in cmake_modules/")
endif()

option(SCSYNTH "Build plugins for scsynth" ON)
option(SUPERNOVA "Build plugins for supernova" OFF)   # OFF avoids needing the nova-tt submodule
option(NATIVE "Optimize for native architecture" OFF)
option(STRICT "Use strict warning flags" OFF)
option(BUILD_UNIVERSAL "Build a universal binary (arm64 + x86_64) on macOS" ON)

# Point at a source tree whose plugin ABI matches the scsynth you run (v3 = 3.12-3.14.x).
set(SC_PATH "../../supercollider-3.14.1" CACHE PATH "Path to SuperCollider source")
sc_check_sc_path("${SC_PATH}")
set(SC_PATH "${SC_PATH}" CACHE PATH "Path to SuperCollider source" FORCE)
include("${SC_PATH}/SCVersion.txt")
message(STATUS "Building plugins for SuperCollider version: ${SC_VERSION}")

project(${project_name})
sc_do_initial_compiler_config()

if(APPLE AND BUILD_UNIVERSAL)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures" FORCE)
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR NOT CMAKE_INSTALL_PREFIX)
    if(APPLE)
        set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/Library/Application Support/SuperCollider/Extensions" CACHE PATH "" FORCE)
    elseif(WIN32)
        set(CMAKE_INSTALL_PREFIX "$ENV{LOCALAPPDATA}/SuperCollider/Extensions" CACHE PATH "" FORCE)
    else()
        set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local/share/SuperCollider/Extensions" CACHE PATH "" FORCE)
    endif()
endif()
message(STATUS "Install directory: ${CMAKE_INSTALL_PREFIX}")

# Add a UGen by adding its name here (expects plugins/<name>.cpp, classes/<name>.sc,
# HelpSource/Classes/<name>.schelp).
foreach(ugen Sediment Silt Talus Scree Loess)
    sc_add_server_plugin(
        "${project_name}"                            # install subdir under the prefix
        "${ugen}"
        "plugins/${ugen}.cpp"
        "classes/${ugen}.sc"
        "HelpSource/Classes/${ugen}.schelp")
endforeach()

message(STATUS "Generating plugin targets done")
