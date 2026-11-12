# JSFX as a library: fxjsfx.
#
# The script engine of this repository (jsfx/effectload.cpp, effectproc.cpp,
# miscfunc.cpp, with EEL2, LICE and SWELL from WDL) built for a host that is
# not REAPER: no window, no editor (jsfx/jsfx_host_embed.cpp takes the place
# of sfxui.cpp). The API is jsfx/jsfx_embed.h. Made for FxmeNet
# (github.com/odoare/FxmeNet), which adds this folder with add_subdirectory().
#
#   cmake -S . -B build
#   cmake --build build --target jsfx_run -j2
#   build/jsfx_run ~/.config/REAPER utility/volume 0=-6
#
# Needs: nasm on x86-64 Linux (EEL2's code generator glue), freetype and
# fontconfig on Linux (the text of @gfx). REAPER's own jsfx.so is still built
# by jsfx/Makefile, untouched.
#
# State: Linux x86-64 is built and tested. The macOS and Windows parts below
# are written from WDL's makefiles and have not been built yet.

cmake_minimum_required(VERSION 3.22)
project(fxjsfx LANGUAGES C CXX)

set(JSFX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jsfx)
set(WDL_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/WDL)

# ── The engine ────────────────────────────────────────────────────────────────
set(FXJSFX_SOURCES
    ${JSFX_DIR}/effectload.cpp
    ${JSFX_DIR}/effectproc.cpp
    ${JSFX_DIR}/miscfunc.cpp
    ${JSFX_DIR}/jsfx_host_embed.cpp)

# ── EEL2 ──────────────────────────────────────────────────────────────────────
set(EEL2_SOURCES
    ${WDL_DIR}/eel2/nseel-lextab.c
    ${WDL_DIR}/eel2/nseel-cfunc.c
    ${WDL_DIR}/eel2/nseel-caltab.c
    ${WDL_DIR}/eel2/nseel-compiler.c
    ${WDL_DIR}/eel2/nseel-eval.c
    ${WDL_DIR}/eel2/nseel-ram.c
    ${WDL_DIR}/eel2/nseel-yylex.c
    ${WDL_DIR}/fft.c)

# ── LICE (the drawing of @gfx) and the PNG reader for gfx_loadimg ─────────────
set(LICE_SOURCES
    ${WDL_DIR}/lice/lice.cpp
    ${WDL_DIR}/lice/lice_arc.cpp
    ${WDL_DIR}/lice/lice_line.cpp
    ${WDL_DIR}/lice/lice_text.cpp
    ${WDL_DIR}/lice/lice_textnew.cpp
    ${WDL_DIR}/lice/lice_image.cpp
    ${WDL_DIR}/lice/lice_png.cpp
    ${WDL_DIR}/lice/lice_bmp.cpp
    ${WDL_DIR}/lice/lice_colorspace.cpp
    ${WDL_DIR}/lice/lice_palette.cpp
    ${WDL_DIR}/lice/lice_texgen.cpp)

file(GLOB PNG_SOURCES ${WDL_DIR}/libpng/*.c)
file(GLOB ZLIB_SOURCES ${WDL_DIR}/zlib/*.c)
list(FILTER PNG_SOURCES  EXCLUDE REGEX "pngtest\\.c$")
list(FILTER ZLIB_SOURCES EXCLUDE REGEX "(example|minigzip)\\.c$")

# ── SWELL: the Win32 calls the engine and LICE make, where there is no Win32 ──
set(SWELL_SOURCES)
if(APPLE)
    list(APPEND SWELL_SOURCES
        ${WDL_DIR}/swell/swell.cpp
        ${WDL_DIR}/swell/swell-ini.cpp
        ${WDL_DIR}/swell/swell-dlg.mm
        ${WDL_DIR}/swell/swell-gdi.mm
        ${WDL_DIR}/swell/swell-kb.mm
        ${WDL_DIR}/swell/swell-menu.mm
        ${WDL_DIR}/swell/swell-misc.mm
        ${WDL_DIR}/swell/swell-miscdlg.mm
        ${WDL_DIR}/swell/swell-wnd.mm)
elseif(NOT WIN32)
    # SWELL without GDK: no window system, LICE draws, freetype for the text
    list(APPEND SWELL_SOURCES
        ${WDL_DIR}/swell/swell.cpp
        ${WDL_DIR}/swell/swell-ini.cpp
        ${WDL_DIR}/swell/swell-miscdlg-generic.cpp
        ${WDL_DIR}/swell/swell-wnd-generic.cpp
        ${WDL_DIR}/swell/swell-menu-generic.cpp
        ${WDL_DIR}/swell/swell-kb-generic.cpp
        ${WDL_DIR}/swell/swell-dlg-generic.cpp
        ${WDL_DIR}/swell/swell-gdi-generic.cpp
        ${WDL_DIR}/swell/swell-misc-generic.cpp
        ${WDL_DIR}/swell/swell-gdi-lice.cpp
        ${WDL_DIR}/swell/swell-generic-headless.cpp)
endif()

add_library(fxjsfx STATIC
    ${FXJSFX_SOURCES} ${EEL2_SOURCES} ${LICE_SOURCES} ${PNG_SOURCES} ${ZLIB_SOURCES} ${SWELL_SOURCES})

target_include_directories(fxjsfx PUBLIC ${JSFX_DIR})

set_target_properties(fxjsfx PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_compile_definitions(fxjsfx PRIVATE
    WDL_FFT_REALSIZE=8
    JSFX_EMBED
    NDEBUG)

# WDL's FFT here works on doubles (WDL_FFT_REALSIZE=8). A host that carries
# its own WDL FFT on floats under the same names (FxmeTools does) would
# otherwise share one of the two with this library, silently: its names are
# changed here.
foreach(fn WDL_fft_init WDL_fft_complexmul WDL_fft_complexmul2 WDL_fft_complexmul3
           WDL_fft WDL_real_fft WDL_fft_permute WDL_fft_permute_tab)
    target_compile_definitions(fxjsfx PRIVATE ${fn}=fxjsfx_${fn})
endforeach()

if(MSVC)
    target_compile_definitions(fxjsfx PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    target_compile_options(fxjsfx PRIVATE /W0)
else()
    # WDL's own warnings are not this build's to fix.
    target_compile_options(fxjsfx PRIVATE -w -fno-math-errno)
    # The engine's headers define curses macros (move, erase, ...) that break
    # the C++11 standard headers: built as C++03, as jsfx/Makefile does.
    set_source_files_properties(${FXJSFX_SOURCES} PROPERTIES COMPILE_OPTIONS "-std=c++03")
    target_compile_definitions(fxjsfx PRIVATE FORCE_WIN32_CURSES)
endif()

# EEL2's glue between compiled code and C: assembly on x86-64, C elsewhere.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND NOT APPLE)
    if(WIN32)
        target_sources(fxjsfx PRIVATE ${WDL_DIR}/eel2/asm-nseel-x64.obj)
        set_source_files_properties(${WDL_DIR}/eel2/asm-nseel-x64.obj PROPERTIES EXTERNAL_OBJECT ON)
    else()
        enable_language(ASM_NASM)
        target_sources(fxjsfx PRIVATE ${WDL_DIR}/eel2/asm-nseel-x64-sse.asm)
        set_source_files_properties(${WDL_DIR}/eel2/asm-nseel-x64-sse.asm PROPERTIES
            LANGUAGE ASM_NASM
            COMPILE_DEFINITIONS AMD64ABI)
        # The assembly has no stack note: without this the linker marks every
        # binary that holds it as needing an executable stack.
        target_link_options(fxjsfx INTERFACE -Wl,-z,noexecstack)
        # Its symbols (nseel_asm_*) are global whatever -fvisibility says; in a
        # plugin hosted by REAPER, whose jsfx.so has the same names, ours could
        # bind to REAPER's. Nothing of this library is exported from a binary
        # that links it.
        target_link_options(fxjsfx INTERFACE -Wl,--exclude-libs,libfxjsfx.a)
    endif()
elseif(APPLE)
    # Both slices of a universal build: x86-64 from the prebuilt object, arm64
    # from C (asm-nseel-aarch64-gcc.c, included by nseel-compiler.c).
    target_sources(fxjsfx PRIVATE ${WDL_DIR}/eel2/asm-nseel-x64-macho.o)
    set_source_files_properties(${WDL_DIR}/eel2/asm-nseel-x64-macho.o PROPERTIES EXTERNAL_OBJECT ON)
endif()

if(APPLE)
    # Objective-C class names are global in a process: REAPER and other
    # plugins carry their own SWELL.
    target_compile_definitions(fxjsfx PRIVATE SWELL_APP_PREFIX=SWELL_FXJSFX_)
    set_source_files_properties(${SWELL_SOURCES} PROPERTIES COMPILE_OPTIONS "-fno-objc-arc")
    target_link_libraries(fxjsfx PUBLIC "-framework Cocoa" "-framework Carbon" "-framework Metal" "-framework MetalKit")
elseif(WIN32)
    target_sources(fxjsfx PRIVATE ${WDL_DIR}/win32_utf8.c)
    target_link_libraries(fxjsfx PUBLIC comctl32)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
    pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
    target_compile_definitions(fxjsfx PRIVATE SWELL_LICE_GDI SWELL_FREETYPE SWELL_FONTCONFIG)
    target_link_libraries(fxjsfx PUBLIC PkgConfig::FREETYPE PkgConfig::FONTCONFIG pthread dl)
endif()

# ── jsfx_run: the engine's test (a sine through a script, one @gfx frame) ─────
option(FXJSFX_BUILD_TOOLS "Build jsfx_run" ${PROJECT_IS_TOP_LEVEL})
if(FXJSFX_BUILD_TOOLS)
    add_executable(jsfx_run tools/jsfx_run.cpp)
    target_link_libraries(jsfx_run PRIVATE fxjsfx)
    if(MSVC)
        target_compile_definitions(jsfx_run PRIVATE _USE_MATH_DEFINES)
    endif()
endif()
