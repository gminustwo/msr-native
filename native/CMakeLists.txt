# msr-native: Samus Returns on a Woohoo! Flight School-style runtime (no emulator). The game's code is the module
# the launcher builds from the player's dump (tools/msr/recomp/build_module.py); this program contains none of it.
#   cmake -S native -B build-native -G Ninja && cmake --build build-native
# Compilers: GCC or Clang (MinGW-w64 on Windows). Needs SDL2, Vulkan headers and loader, glslc.
cmake_minimum_required(VERSION 3.16)
project(msr_native C CXX)
set(MSR_NATIVE_VERSION "0.1.0")   # the launcher shows it; the native code's module.json records it
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(MSR_AOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/arm/msr_aot)
# The module is accepted only when it was compiled against the same aot_runtime.h (as src/core/CMakeLists.txt checks).
file(READ ${MSR_AOT_DIR}/aot_runtime.h MSR_AOT_RUNTIME_TEXT)
string(REPLACE "\r" "" MSR_AOT_RUNTIME_TEXT "${MSR_AOT_RUNTIME_TEXT}")
string(SHA256 MSR_AOT_RUNTIME_SHA256 "${MSR_AOT_RUNTIME_TEXT}")
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)   # inside the main build, src/core already depends on it
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MSR_AOT_DIR}/aot_runtime.h)
endif()

find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)
# SDL2 built from a source folder (static: nothing to ship), e.g. for an AppImage on a system without SDL2's headers
set(MSR_SDL2_SOURCE "" CACHE PATH "Build SDL2 statically from this source folder instead of using the system's")
if (MSR_SDL2_SOURCE AND NOT TARGET SDL2::SDL2)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
    set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
    add_subdirectory(${MSR_SDL2_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/sdl2 EXCLUDE_FROM_ALL)
    add_library(SDL2::SDL2 ALIAS SDL2-static)
endif()
if (NOT TARGET SDL2::SDL2)   # inside the main build the system SDL2 may already be found
    find_package(SDL2 REQUIRED)
endif()
find_program(GLSLC glslc)
if (NOT GLSLC)
    # glslangValidator instead (Linux build environments often have only glslang-tools): SPIR-V, then the same C
    # initializer glslc -mfmt=c writes (cmake/spv2inc.cmake)
    find_program(GLSLANG glslangValidator REQUIRED)
endif()

set(SH ${CMAKE_CURRENT_SOURCE_DIR}/runtime/shaders)
set(SHO ${CMAKE_CURRENT_BINARY_DIR}/shaders)
# out: the .inc name; src: the shader; extra: defines
function(msr_shader out src)
  if (GLSLC)
    add_custom_command(OUTPUT ${SHO}/${out}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SHO}
      COMMAND ${GLSLC} --target-env=vulkan1.3 -O ${ARGN} -mfmt=c -o ${SHO}/${out} ${SH}/${src}
      DEPENDS ${SH}/${src} ${SH}/fsr/ffx_a.h ${SH}/fsr/ffx_fsr1.h)
  else()
    add_custom_command(OUTPUT ${SHO}/${out}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SHO}
      COMMAND ${GLSLANG} -V --target-env vulkan1.3 ${ARGN} -o ${SHO}/${out}.spv ${SH}/${src}
      COMMAND ${CMAKE_COMMAND} -DIN=${SHO}/${out}.spv -DOUT=${SHO}/${out} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/spv2inc.cmake
      DEPENDS ${SH}/${src} ${SH}/fsr/ffx_a.h ${SH}/fsr/ffx_fsr1.h ${CMAKE_CURRENT_SOURCE_DIR}/cmake/spv2inc.cmake)
  endif()
endfunction()
foreach(sh pica.vert pica_vs.vert pica.frag present.vert present.frag fsr_easu.frag fsr_rcas.frag msr.vert msr.frag hdr.frag)
  msr_shader(${sh}.inc ${sh})
endforeach()
msr_shader(pica_nd.frag.inc pica.frag -DNO_FRAG_DEPTH)

add_executable(msr-native
  runtime/main.c runtime/kernel.c runtime/services.c runtime/fs.c runtime/gsp.c runtime/pica.c runtime/video.c
  runtime/dsp.c runtime/pica_sw.c runtime/input.c runtime/pica_vk.c runtime/pica_vsgen.c runtime/runtime.c
  runtime/stubs.c runtime/interp.c runtime/interp_full.c runtime/dispatch.cpp runtime/state.c
  runtime/msr/settings.cpp runtime/msr/patches.cpp runtime/msr/compositor.cpp runtime/msr/cheats.cpp runtime/msr/camera_fov.cpp runtime/msr/glyphs.cpp
  runtime/msr/textures.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/cityhash.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/msr/touch_nav.cpp
  runtime/msr/ini.cpp runtime/msr/mods.cpp runtime/msr/patch.cpp runtime/msr/layered_fs.cpp runtime/msr/overlay_host.cpp
  runtime/msr/launcher.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/msr/overlay.cpp
  ${SHO}/pica.vert.inc ${SHO}/pica_vs.vert.inc ${SHO}/pica.frag.inc
  ${SHO}/present.vert.inc ${SHO}/present.frag.inc
  ${SHO}/fsr_easu.frag.inc ${SHO}/fsr_rcas.frag.inc ${SHO}/pica_nd.frag.inc ${SHO}/msr.vert.inc ${SHO}/msr.frag.inc
  ${SHO}/hdr.frag.inc)
# Samus Returns' features share src/msr's headers with the emulator-based build (msr/compat: the types they use).
target_include_directories(msr-native PRIVATE runtime/msr/compat runtime ${SHO} ${MSR_AOT_DIR}
                           ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(msr-native PRIVATE _FILE_OFFSET_BITS=64 MSR_AOT_RUNTIME_SHA256="${MSR_AOT_RUNTIME_SHA256}" MSR_NATIVE
                           MSR_NATIVE_VERSION="${MSR_NATIVE_VERSION}")
# Saved vertex shaders (pwr_shaders.bin) stay valid while their generator is the same: its source's hash, not the build time
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/runtime/pica_vsgen.c MSR_VSGEN_TEXT)
string(REPLACE "\r" "" MSR_VSGEN_TEXT "${MSR_VSGEN_TEXT}")
string(SHA256 MSR_VSGEN_SHA256 "${MSR_VSGEN_TEXT}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime/pica_vsgen.c)
set_source_files_properties(runtime/pica_vsgen.c PROPERTIES COMPILE_DEFINITIONS MSR_VSGEN_SHA256="${MSR_VSGEN_SHA256}")
# The settings overlay is the emulator-based build's (src/msr/overlay.cpp) on Dear ImGui (externals/msr-imgui, unmodified
# upstream, MIT), its Vulkan functions loaded at run time as there.
set(MSR_IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../externals/msr-imgui)
add_library(msr_native_imgui STATIC ${MSR_IMGUI_DIR}/imgui.cpp ${MSR_IMGUI_DIR}/imgui_draw.cpp ${MSR_IMGUI_DIR}/imgui_tables.cpp
            ${MSR_IMGUI_DIR}/imgui_widgets.cpp ${MSR_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(msr_native_imgui SYSTEM PUBLIC ${MSR_IMGUI_DIR})
target_compile_definitions(msr_native_imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_link_libraries(msr_native_imgui PUBLIC Vulkan::Vulkan)
target_link_libraries(msr-native PRIVATE msr_native_imgui)
target_compile_options(msr-native PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-unused-result>)
target_link_libraries(msr-native PRIVATE Threads::Threads Vulkan::Vulkan SDL2::SDL2 m ${CMAKE_DL_LIBS})
# Save states are compressed with zlib when it is there (it comes with libpng)
find_package(ZLIB)
if (ZLIB_FOUND)
    target_compile_definitions(msr-native PRIVATE MSR_HAVE_ZLIB)
    target_link_libraries(msr-native PRIVATE ZLIB::ZLIB)
endif()
# Texture packs need libpng (MSYS2: mingw-w64-ucrt-x86_64-libpng; Linux: libpng-dev); without it they are left out.
find_package(PNG)
if (PNG_FOUND)
    target_compile_definitions(msr-native PRIVATE MSR_HAVE_PNG)
    target_link_libraries(msr-native PRIVATE PNG::PNG)
else()
    message(STATUS "msr-native: no libpng, texture packs are left out")
endif()
if(WIN32)
  enable_language(RC)
  if(MINGW AND CMAKE_HOST_WIN32)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --preprocessor=gcc --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED")
  endif()
  target_sources(msr-native PRIVATE runtime/win/pwr.rc runtime/win/hdr_win.c)
  set_source_files_properties(runtime/win/pwr.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/win/pwr.manifest;${CMAKE_CURRENT_SOURCE_DIR}/runtime/win/msr-native.ico")
  target_link_libraries(msr-native PRIVATE winmm shell32 dxgi dwmapi comdlg32)
  # a window program: no console opens with it (the launcher logs to user/launcher.log, the game to the file it is given)
  set_target_properties(msr-native PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
