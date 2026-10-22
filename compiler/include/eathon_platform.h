/*
 * ============================================================================
 * EATHON PLATFORM ABSTRACTION LAYER (eathon_platform.h)
 * Separates OS-specific filesystem, executable permissions, host compiler
 * invocation, and shared-library (Android JNI) configuration from the
 * platform-independent Eathon compiler & database core.
 *
 * Supported Targets:
 *   - Windows 10/11 (x86_64 / ARM64 via MinGW-w64 GCC, LLVM Clang, or MSVC)
 *   - Linux (x86_64 / AArch64 via GCC or Clang, plus direct ELF64 emitter)
 *   - macOS (x86_64 / Apple Silicon ARM64 via Clang)
 *   - Android (ARM64-v8a, armeabi-v7a, x86_64 via Android NDK & JNI)
 * ============================================================================
 */

#ifndef EATHON_PLATFORM_H
#define EATHON_PLATFORM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
    #define EATHON_OS_WINDOWS 1
    #define EATHON_OS_NAME "Windows-x86_64"
    #define EATHON_PATH_SEP '\\'
    #define EATHON_DEFAULT_BIN_OUT "eathon_out.exe"
    #define EATHON_DEFAULT_LL_OUT  "eathon_out.ll"
    #define EATHON_DEFAULT_C_OUT   "eathon_out.c"
    #include <io.h>
    #define EATHON_SET_EXECUTABLE(path) ((void)0)
#elif defined(__ANDROID__)
    #define EATHON_OS_ANDROID 1
    #define EATHON_OS_NAME "Android-NDK"
    #define EATHON_PATH_SEP '/'
    #define EATHON_DEFAULT_BIN_OUT "/data/local/tmp/eathon_out"
    #define EATHON_DEFAULT_LL_OUT  "/data/local/tmp/eathon_out.ll"
    #define EATHON_DEFAULT_C_OUT   "/data/local/tmp/eathon_out.c"
    #include <sys/stat.h>
    #define EATHON_SET_EXECUTABLE(path) chmod((path), 0755)
#elif defined(__APPLE__) && defined(__MACH__)
    #define EATHON_OS_MACOS 1
    #define EATHON_OS_NAME "macOS"
    #define EATHON_PATH_SEP '/'
    #define EATHON_DEFAULT_BIN_OUT "/tmp/eathon_out"
    #define EATHON_DEFAULT_LL_OUT  "/tmp/eathon_out.ll"
    #define EATHON_DEFAULT_C_OUT   "/tmp/eathon_out.c"
    #include <sys/stat.h>
    #define EATHON_SET_EXECUTABLE(path) chmod((path), 0755)
#else
    #define EATHON_OS_LINUX 1
    #define EATHON_OS_NAME "Linux-x86_64"
    #define EATHON_PATH_SEP '/'
    #define EATHON_DEFAULT_BIN_OUT "/tmp/eathon_out"
    #define EATHON_DEFAULT_LL_OUT  "/tmp/eathon_out.ll"
    #define EATHON_DEFAULT_C_OUT   "/tmp/eathon_out.c"
    #include <sys/stat.h>
    #define EATHON_SET_EXECUTABLE(path) chmod((path), 0755)
#endif

#define EATHON_PLATFORM_NAME EATHON_OS_NAME

#if defined(__aarch64__) || defined(_M_ARM64)
    #define EATHON_ARCH_NAME "arm64-v8a"
#elif defined(__arm__) || defined(_M_ARM)
    #define EATHON_ARCH_NAME "armeabi-v7a"
#elif defined(__x86_64__) || defined(_M_X64)
    #define EATHON_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
    #define EATHON_ARCH_NAME "x86"
#else
    #define EATHON_ARCH_NAME "native"
#endif

#endif /* EATHON_PLATFORM_H */

