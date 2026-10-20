# Fork modification 2026-10-10: default-off public observation build options.
# See LICENSE-NOTICES.md; existing upstream license retained.
cmake_minimum_required ( VERSION 3.5 )

project ( OpenXcom )

set ( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules" )
include(BuildType)
include(GNUInstallDirs)

# For yaml-cpp
set (CMAKE_CXX_STANDARD 11)
# use std=c++11 rather than std=gnu++11
set (CMAKE_CXX_EXTENSIONS OFF)

# SPDX-License-Identifier: GPL-3.0-or-later; unintegrated loader backend, 2026-10-14.
# These helper switches do not activate Game by themselves. The separate
# responsive integration requires all three and remains default OFF/native-gated.
# OFF retains the legacy Game loading route, not a responsive fallback.
option ( OPENXCOM_BUILD_LINUX_LOADER_BACKEND "Build the unintegrated Linux/glibc loader backend" OFF )
option ( OPENXCOM_BUILD_LOADING_PRESENTATION_HELPERS "Build unactivated loading presentation and logging helpers" OFF )
option ( OPENXCOM_BUILD_LOADING_AUDIO_HANDOFF "Build the unactivated loading audio handoff" OFF )
option ( OPENXCOM_BUILD_RESPONSIVE_LOADING "Build staged responsive Game loading and fixed-generation video (native release gated)" OFF )
if ( OPENXCOM_BUILD_LOADING_AUDIO_HANDOFF AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  message ( FATAL_ERROR "Loading audio handoff requires the selected Linux profile" )
endif()
if ( OPENXCOM_BUILD_LINUX_LOADER_BACKEND AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  message ( FATAL_ERROR "The Linux loader backend requires Linux/glibc" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; optional observer addition, 2026-10-10.
option ( EXPERIMENTAL_MIXED_AUDIO_OBSERVER "Build experimental final mixed-audio observer (POSIX only)" OFF )
if ( EXPERIMENTAL_MIXED_AUDIO_OBSERVER AND NOT UNIX )
  message ( FATAL_ERROR "Experimental mixed-audio observer currently requires POSIX" )
endif()

option ( EXPERIMENTAL_CURSOR_OBSERVER "Build experimental final presented cursor-pixel observer (POSIX only)" OFF )
if ( EXPERIMENTAL_CURSOR_OBSERVER AND NOT UNIX )
  message ( FATAL_ERROR "Experimental cursor observer currently requires POSIX" )
endif()

option ( EXPERIMENTAL_HUD_OBSERVER "Build the experimental read-only public HUD observer (POSIX only)" OFF )
if ( EXPERIMENTAL_HUD_OBSERVER AND NOT UNIX )
  message ( FATAL_ERROR "Experimental HUD observer currently requires POSIX" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-10.
option ( EXPERIMENTAL_BATTLE_PAIR_GATE "Build isolated hidden-occupant render test gate" OFF )
if ( EXPERIMENTAL_BATTLE_PAIR_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER )
  message ( FATAL_ERROR "Battle pair gate requires EXPERIMENTAL_CURSOR_OBSERVER" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-10.
option ( EXPERIMENTAL_OBSERVER_EFFECTS_GATE "Build isolated observer request-effects test gate" OFF )
if ( EXPERIMENTAL_OBSERVER_EFFECTS_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER )
  message ( FATAL_ERROR "Observer effects gate requires EXPERIMENTAL_CURSOR_OBSERVER" )
endif()
if ( EXPERIMENTAL_OBSERVER_EFFECTS_GATE AND EXPERIMENTAL_BATTLE_PAIR_GATE )
  message ( FATAL_ERROR "Observer effects and render pair gates are separate test binaries" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-10.
option ( EXPERIMENTAL_NATURAL_FOV_GATE "Build isolated natural-FOV/pause lifecycle test gate" OFF )
if ( EXPERIMENTAL_NATURAL_FOV_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER )
  message ( FATAL_ERROR "Natural FOV gate requires EXPERIMENTAL_CURSOR_OBSERVER" )
endif()
if ( EXPERIMENTAL_NATURAL_FOV_GATE AND (EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_OBSERVER_EFFECTS_GATE OR EXPERIMENTAL_HUD_OBSERVER) )
  message ( FATAL_ERROR "Natural FOV gate requires a separate test binary" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-10.
option ( EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE "Build isolated battlefield mixed-audio test gate" OFF )
if ( EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE AND NOT EXPERIMENTAL_MIXED_AUDIO_OBSERVER )
  message ( FATAL_ERROR "Battlefield audio gate requires EXPERIMENTAL_MIXED_AUDIO_OBSERVER" )
endif()
if ( EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE AND (EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_OBSERVER_EFFECTS_GATE OR EXPERIMENTAL_NATURAL_FOV_GATE OR EXPERIMENTAL_CURSOR_OBSERVER OR EXPERIMENTAL_HUD_OBSERVER) )
  message ( FATAL_ERROR "Battlefield audio gate requires a separate test binary" )
endif()
if ( EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  message ( FATAL_ERROR "Battlefield audio gate requires the Linux linker postmix wrapper" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-11.
option ( EXPERIMENTAL_SMOKE_DENSITY_GATE "Build isolated smoke-density render equivalence test gate" OFF )
if ( EXPERIMENTAL_SMOKE_DENSITY_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER )
  message ( FATAL_ERROR "Smoke density gate requires EXPERIMENTAL_CURSOR_OBSERVER" )
endif()
if ( EXPERIMENTAL_SMOKE_DENSITY_GATE AND (EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_OBSERVER_EFFECTS_GATE OR EXPERIMENTAL_NATURAL_FOV_GATE OR EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE OR EXPERIMENTAL_HUD_OBSERVER OR EXPERIMENTAL_MIXED_AUDIO_OBSERVER) )
  message ( FATAL_ERROR "Smoke density gate requires a separate test binary" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-11.
option ( EXPERIMENTAL_SMOKE_CLIPPING_GATE "Build isolated smoke viewport-clipping equivalence test gate" OFF )
if ( EXPERIMENTAL_SMOKE_CLIPPING_GATE AND NOT EXPERIMENTAL_CURSOR_OBSERVER )
  message ( FATAL_ERROR "Smoke clipping gate requires EXPERIMENTAL_CURSOR_OBSERVER" )
endif()
if ( EXPERIMENTAL_SMOKE_CLIPPING_GATE AND (EXPERIMENTAL_SMOKE_DENSITY_GATE OR EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_OBSERVER_EFFECTS_GATE OR EXPERIMENTAL_NATURAL_FOV_GATE OR EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE OR EXPERIMENTAL_HUD_OBSERVER OR EXPERIMENTAL_MIXED_AUDIO_OBSERVER) )
  message ( FATAL_ERROR "Smoke clipping gate requires a separate test binary" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; test-only addition, 2026-10-11.
option ( EXPERIMENTAL_TIMING_CALIBRATION "Build isolated native paired timing calibration" OFF )
if ( EXPERIMENTAL_TIMING_CALIBRATION AND NOT EXPERIMENTAL_CURSOR_OBSERVER )
  message ( FATAL_ERROR "Timing calibration requires EXPERIMENTAL_CURSOR_OBSERVER" )
endif()
if ( EXPERIMENTAL_TIMING_CALIBRATION AND (EXPERIMENTAL_SMOKE_CLIPPING_GATE OR EXPERIMENTAL_SMOKE_DENSITY_GATE OR EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_OBSERVER_EFFECTS_GATE OR EXPERIMENTAL_NATURAL_FOV_GATE OR EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE OR EXPERIMENTAL_HUD_OBSERVER OR EXPERIMENTAL_MIXED_AUDIO_OBSERVER) )
  message ( FATAL_ERROR "Timing calibration requires a separate test binary" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; distinct native agent-control profile, 2026-10-18.
option ( EXPERIMENTAL_AGENT_CONTROL "Build bounded semantic native agent control (separate opt-in ABI)" OFF )
if ( EXPERIMENTAL_AGENT_CONTROL AND (NOT UNIX OR NOT OPENXCOM_BUILD_LOADING_PRESENTATION_HELPERS) )
  message ( FATAL_ERROR "Agent control requires POSIX and OPENXCOM_BUILD_LOADING_PRESENTATION_HELPERS" )
endif()
if ( EXPERIMENTAL_AGENT_CONTROL AND EXPERIMENTAL_MIXED_AUDIO_OBSERVER )
  message ( FATAL_ERROR "Agent control and the independent mixed-audio observer require separate postmix-owning builds" )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; supervised pilot, 2026-10-11.
option ( EXPERIMENTAL_SUPERVISED_STANCE "Build local-confirmation desired-stance pilot" OFF )
if ( EXPERIMENTAL_SUPERVISED_STANCE AND NOT EXPERIMENTAL_CURSOR_OBSERVER )
  message ( FATAL_ERROR "Supervised stance requires EXPERIMENTAL_CURSOR_OBSERVER" )
endif()
if ( EXPERIMENTAL_AGENT_CONTROL AND EXPERIMENTAL_SUPERVISED_STANCE )
  message ( FATAL_ERROR "Agent control and supervised stance are distinct build profiles" )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; supervised selection, 2026-10-12.
option ( EXPERIMENTAL_SUPERVISED_SELECTION "Build local-confirmation next/previous selection pilot" OFF )
if ( EXPERIMENTAL_SUPERVISED_SELECTION AND NOT EXPERIMENTAL_SUPERVISED_STANCE )
  message ( FATAL_ERROR "Supervised selection requires EXPERIMENTAL_SUPERVISED_STANCE" )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; supervised turn, 2026-10-12.
option ( EXPERIMENTAL_SUPERVISED_TURN "Build local-confirmation normal right-click pilot" OFF )
if ( EXPERIMENTAL_SUPERVISED_TURN AND NOT EXPERIMENTAL_SUPERVISED_STANCE )
  message ( FATAL_ERROR "Supervised turn requires EXPERIMENTAL_SUPERVISED_STANCE" )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; reconstructed Snapshot foundations, 2026-10-13.
option ( EXPERIMENTAL_SUPERVISED_SNAPSHOT "Build experimental local-confirmation Snapshot foundations (POSIX only)" OFF )
if ( EXPERIMENTAL_SUPERVISED_SNAPSHOT AND NOT UNIX )
  message ( FATAL_ERROR "Supervised Snapshot currently requires POSIX" )
endif()
if ( EXPERIMENTAL_SUPERVISED_SNAPSHOT AND (NOT EXPERIMENTAL_SUPERVISED_TURN OR NOT EXPERIMENTAL_SUPERVISED_STANCE OR NOT EXPERIMENTAL_CURSOR_OBSERVER) )
  message ( FATAL_ERROR "Supervised Snapshot requires supervised turn, stance and cursor observer infrastructure" )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; supervised map left-click, 2026-10-12.
option ( EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK "Build local-confirmation ordinary map left-click pilot" OFF )
if ( EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK AND NOT EXPERIMENTAL_SUPERVISED_TURN )
  message ( FATAL_ERROR "Supervised map left-click requires EXPERIMENTAL_SUPERVISED_TURN" )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; supervised inventory opening, 2026-10-12.
option ( EXPERIMENTAL_SUPERVISED_INVENTORY "Build local-confirmation inventory opening and capless presented inventory view" OFF )
if ( EXPERIMENTAL_SUPERVISED_INVENTORY AND NOT EXPERIMENTAL_SUPERVISED_TURN )
  message ( FATAL_ERROR "Supervised inventory requires EXPERIMENTAL_SUPERVISED_TURN" )
endif()
# SPDX-License-Identifier: GPL-3.0-or-later; supervised inventory closing, 2026-10-12.
option ( EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE "Build local-confirmation ordinary inventory OK/close" OFF )
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE AND NOT EXPERIMENTAL_SUPERVISED_INVENTORY )
  message ( FATAL_ERROR "Supervised inventory close requires EXPERIMENTAL_SUPERVISED_INVENTORY" )
endif()
option ( EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND "Build local-confirmation ordinary inventory Ground action" OFF )
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND AND NOT EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE )
  message ( FATAL_ERROR "Supervised inventory ground requires EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE" )
endif()
option ( EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK "Build local-confirmation ordinary inventory left-click" OFF )
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK AND NOT EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE )
  message ( FATAL_ERROR "Supervised inventory click requires EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE" )
endif()
option ( EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK "Build local-confirmation ordinary inventory right-click" OFF )
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK AND NOT EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE )
  message ( FATAL_ERROR "Supervised inventory right-click requires EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE" )
endif()
option ( EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD "Build local-confirmation ordinary inventory Unload action" OFF )
if ( EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD AND NOT EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE )
  message ( FATAL_ERROR "Supervised inventory Unload requires EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE" )
endif()
option ( EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE "Build isolated finite inventory-Unload control test gate" OFF )
if ( EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE AND (NOT EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD OR NOT EXPERIMENTAL_SUPERVISED_SELECTION OR NOT EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK OR NOT EXPERIMENTAL_STANCE_CONTROL_GATE) )
  message ( FATAL_ERROR "Inventory Unload control gate requires supervised Unload, selection, map-left-click and stance gate" )
endif()
option ( EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE "Build isolated finite inventory-right-click control test gate" OFF )
if ( EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE AND (NOT EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK OR NOT EXPERIMENTAL_SUPERVISED_SELECTION OR NOT EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK OR NOT EXPERIMENTAL_STANCE_CONTROL_GATE) )
  message ( FATAL_ERROR "Inventory right-click control gate requires supervised right-click, selection, map-left-click and stance gate" )
endif()
option ( EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE "Build isolated finite inventory-click control test gate" OFF )
if ( EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE AND (NOT EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK OR NOT EXPERIMENTAL_SUPERVISED_SELECTION OR NOT EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK OR NOT EXPERIMENTAL_STANCE_CONTROL_GATE) )
  message ( FATAL_ERROR "Inventory-click control gate requires supervised inventory click, selection, map left-click and stance control gate" )
endif()
option ( EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE "Build isolated finite inventory-ground control test gate" OFF )
if ( EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE AND (NOT EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND OR NOT EXPERIMENTAL_SUPERVISED_SELECTION OR NOT EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK OR NOT EXPERIMENTAL_STANCE_CONTROL_GATE) )
  message ( FATAL_ERROR "Inventory-ground control gate requires supervised inventory ground, selection, map left-click and stance control gate" )
endif()
option ( EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE "Build isolated finite inventory-close control test gate" OFF )
if ( EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE AND (NOT EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE OR NOT EXPERIMENTAL_SUPERVISED_SELECTION OR NOT EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK OR NOT EXPERIMENTAL_STANCE_CONTROL_GATE) )
  message ( FATAL_ERROR "Inventory-close control gate requires supervised inventory close, selection, map left-click and stance control gate" )
endif()
option ( EXPERIMENTAL_STANCE_CONTROL_GATE "Build isolated finite stance-control test gate" OFF )
if ( EXPERIMENTAL_STANCE_CONTROL_GATE AND NOT EXPERIMENTAL_SUPERVISED_STANCE )
  message ( FATAL_ERROR "Stance control gate requires EXPERIMENTAL_SUPERVISED_STANCE" )
endif()
if ( EXPERIMENTAL_STANCE_CONTROL_GATE AND (EXPERIMENTAL_TIMING_CALIBRATION OR EXPERIMENTAL_SMOKE_CLIPPING_GATE OR EXPERIMENTAL_SMOKE_DENSITY_GATE OR EXPERIMENTAL_BATTLE_PAIR_GATE OR EXPERIMENTAL_OBSERVER_EFFECTS_GATE OR EXPERIMENTAL_NATURAL_FOV_GATE OR EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE) )
  message ( FATAL_ERROR "Stance control gate requires a separate test binary" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; finite supervised-turn gate, 2026-10-12.
option ( EXPERIMENTAL_TURN_CONTROL_GATE "Build isolated finite turn-control test gate" OFF )
if ( EXPERIMENTAL_TURN_CONTROL_GATE AND (NOT EXPERIMENTAL_SUPERVISED_TURN OR NOT EXPERIMENTAL_SUPERVISED_SELECTION OR NOT EXPERIMENTAL_STANCE_CONTROL_GATE) )
  message ( FATAL_ERROR "Turn control gate requires supervised turn, selection and stance control gate" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; finite supervised map left-click gate, 2026-10-12.
option ( EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE "Build isolated finite map left-click control test gate" OFF )
if ( EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE AND (NOT EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK OR NOT EXPERIMENTAL_SUPERVISED_SELECTION OR NOT EXPERIMENTAL_STANCE_CONTROL_GATE) )
  message ( FATAL_ERROR "Map left-click control gate requires supervised map left-click, selection and stance control gate" )
endif()

# SPDX-License-Identifier: GPL-3.0-or-later; finite supervised inventory-open gate, 2026-10-12.
option ( EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE "Build isolated finite inventory-open control test gate" OFF )
if ( EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE AND (NOT EXPERIMENTAL_SUPERVISED_INVENTORY OR NOT EXPERIMENTAL_SUPERVISED_SELECTION OR NOT EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK OR NOT EXPERIMENTAL_STANCE_CONTROL_GATE) )
  message ( FATAL_ERROR "Inventory-open control gate requires supervised inventory, selection, map left-click and stance control gate" )
endif()

option ( DEV_BUILD "Development Build. Disable this for release builds" ON )
option ( BUILD_PACKAGE "Prepares build for creation of a package with CPack" ON )
option ( ENABLE_WARNING "Always show warnings (even for release builds)" OFF )
option ( FATAL_WARNING "Treat warnings as errors" OFF )
option ( ENABLE_CLANG_ANALYSIS "When building with clang, enable the static analyzer" OFF )
option ( CHECK_CCACHE "Check if ccache is installed and use it" OFF )
set ( MSVC_WARNING_LEVEL 3 CACHE STRING "Visual Studio warning levels" )
option ( FORCE_INSTALL_DATA_TO_BIN "Force installation of data to binary directory" OFF )
set ( DATADIR "" CACHE STRING "Where to search for datafiles" )
set ( OPENXCOM_VERSION_STRING "" CACHE STRING "Version string (after x.x)" )

if ( CHECK_CCACHE )
  find_program( CCACHE_PROGRAM ccache )
  if( NOT CCACHE_PROGRAM )
    message ( "CCACHE requested but not found on the system." )
  else ()
    set_property( GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}" )
    set_property( GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}" )
    message ( "found CCACHE (${CCACHE_PROGRAM})" )
  endif()
endif ()

if ( WIN32 )
  set ( default_deps_dir "${CMAKE_SOURCE_DIR}/deps" )
endif ()

if ( APPLE )
  set ( MACOS_SDLMAIN_M_PATH "${CMAKE_SOURCE_DIR}/src/apple/SDLMain.m" CACHE STRING "Path to SDLMain.m file" )
  option ( CREATE_BUNDLE "Create a Mac OS application bundle" ON )
  if ( NOT EXISTS "${MACOS_SDLMAIN_M_PATH}" )
    message ( FATAL_ERROR "On Mac OS, SDLMain.m is required. Please set the MACOS_SDLMAIN_M_PATH variable" )
  endif ()
endif ()

set ( DEPS_DIR "${default_deps_dir}" CACHE STRING "Dependencies directory" )

# Add check for library (SDL_gfx, yaml-cpp )
if ( IS_DIRECTORY ${DEPS_DIR}/include )
   include_directories ( SYSTEM ${DEPS_DIR}/include/SDL ${DEPS_DIR}/include/yaml-cpp ${DEPS_DIR}/include )
   if ( CMAKE_CL_64 )
     link_directories ( ${DEPS_DIR}/lib/x64 )
   else ( )
     link_directories ( ${DEPS_DIR}/lib/Win32 )
   endif()
   set( SDL_LIBRARY SDL )
   set ( SDLGFX_LIBRARY SDL_gfx )
   set ( SDLMIXER_LIBRARY SDL_mixer )
   set ( SDLIMAGE_LIBRARY SDL_image )
   set ( YAMLCPP_LIBRARY yaml-cpp )
   set ( YAMLCPP_LIBRARY_DEBUG yaml-cppd )
   add_definitions( -DYAML_CPP_DLL )
else ( )
  find_package ( SDL2 COMPONENTS mixer gfx image)
  find_package ( Yaml_cpp 0.5.3)
  set ( YAMLCPP_LIBRARY_DEBUG ${YAMLCPP_LIBRARY} )

  if ( NOT SDL_FOUND )
    message ( FATAL_ERROR "Can't find SDL which is required" )
  else ()
    include_directories ( SYSTEM ${SDL_INCLUDE_DIR} )
    message ( "found SDL ${SDL_MAJOR}.${SDL_MINOR}.${SDL_MICRO} (${SDL_LIBRARY_DIRS}:${SDL_INCLUDE_DIR})" )
  endif ()

  if ( NOT SDLMIXER_FOUND )
    message ( FATAL_ERROR "Can't find SDL_mixer which is required" )
  else ()
    include_directories ( SYSTEM ${SDLMIXER_INCLUDE_DIR} )
    message ( "found SDL_mixer ${SDL_MIXER_MAJOR}.${SDL_MIXER_MINOR}.${SDL_MIXER_MICRO} (${SDL_MIXER_LIBRARY_DIRS}:${SDLMIXER_INCLUDE_DIR})" )
  endif ()

  if ( NOT SDLGFX_FOUND )
    message ( FATAL_ERROR "Can't find SDL_gfx which is required" )
  else ()
    include_directories ( SYSTEM ${SDLGFX_INCLUDE_DIR} )
    message ( "found SDL_gfx ${SDL_GFX_MAJOR}.${SDL_GFX_MINOR}.${SDL_GFX_MICRO} (${SDL_GFX_LIBRARY_DIRS}:${SDLGFX_INCLUDE_DIR})" )
  endif ()

  if ( NOT SDLIMAGE_FOUND )
    message ( FATAL_ERROR "Can't find SDL_image which is required" )
  else ()
    include_directories ( SYSTEM ${SDLIMAGE_INCLUDE_DIR} )
    message ( "found SDL_image ${SDL_IMAGE_MAJOR}.${SDL_IMAGE_MINOR}.${SDL_IMAGE_MICRO} (${SDL_IMAGE_LIBRARY_DIRS}:${SDLIMAGE_INCLUDE_DIR})" )
  endif ()

  if ( NOT YAMLCPP_FOUND )
    message ( FATAL_ERROR "Can't find yaml-cpp which is required" )
  else ()
    include_directories ( SYSTEM ${YAMLCPP_INCLUDE_DIR} )
    message ( "found yaml-cpp(${YAMLCPP_LIBRARY_DIRS}:${YAMLCPP_INCLUDE_DIR})" )
  endif ( NOT YAMLCPP_FOUND )
endif()

# Keep this independent of the legacy FindSDL2.cmake module above: that module
# intentionally finds SDL 1.2 and its image/mixer/gfx dependencies. These private
# variables select SDL2 only for the raw-pthread TLS-cleanup bridge.
if ( OPENXCOM_BUILD_LINUX_LOADER_BACKEND )
  find_package ( Threads REQUIRED )
  if ( NOT CMAKE_USE_PTHREADS_INIT )
    message ( FATAL_ERROR "The Linux loader backend requires POSIX Threads" )
  endif()
  find_path ( OPENXCOM_LOADER_SDL2_INCLUDE_DIR SDL_thread.h
    HINTS ${DEPS_DIR}
    PATH_SUFFIXES include/SDL2 SDL2 )
  find_library ( OPENXCOM_LOADER_SDL2_LIBRARY
    NAMES SDL2 SDL2-2.0
    HINTS ${DEPS_DIR}
    PATH_SUFFIXES lib lib64 )
  if ( NOT OPENXCOM_LOADER_SDL2_INCLUDE_DIR OR NOT OPENXCOM_LOADER_SDL2_LIBRARY )
    message ( FATAL_ERROR "The Linux loader backend requires SDL2 >= 2.0.16 headers and library" )
  endif()
  set ( OPENXCOM_LOADER_SDL2_INCLUDE_DIRS "${OPENXCOM_LOADER_SDL2_INCLUDE_DIR}" )
  # Debian's SDL_config.h redirects to an architecture-specific SDL2 header.
  # Resolve that root within the selected header installation, never silently
  # borrowing a different host SDL2 config. Other SDL2 layouts need no extra root.
  file ( READ "${OPENXCOM_LOADER_SDL2_INCLUDE_DIR}/SDL_config.h" openxcom_loader_sdl2_config )
  if ( openxcom_loader_sdl2_config MATCHES "SDL2/_real_SDL_config[.]h" )
    get_filename_component ( openxcom_loader_sdl2_include_root
      "${OPENXCOM_LOADER_SDL2_INCLUDE_DIR}" DIRECTORY )
    find_path ( OPENXCOM_LOADER_SDL2_CONFIG_INCLUDE_DIR SDL2/_real_SDL_config.h
      PATHS "${openxcom_loader_sdl2_include_root}/${CMAKE_LIBRARY_ARCHITECTURE}"
            "${openxcom_loader_sdl2_include_root}"
      NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH )
    if ( NOT OPENXCOM_LOADER_SDL2_CONFIG_INCLUDE_DIR OR
         NOT EXISTS "${OPENXCOM_LOADER_SDL2_CONFIG_INCLUDE_DIR}/SDL2/_real_SDL_config.h" )
      message ( FATAL_ERROR "Selected SDL2 headers require their matching architecture config; set OPENXCOM_LOADER_SDL2_CONFIG_INCLUDE_DIR explicitly" )
    endif()
    list ( APPEND OPENXCOM_LOADER_SDL2_INCLUDE_DIRS "${OPENXCOM_LOADER_SDL2_CONFIG_INCLUDE_DIR}" )
  endif()
  mark_as_advanced ( OPENXCOM_LOADER_SDL2_INCLUDE_DIR OPENXCOM_LOADER_SDL2_CONFIG_INCLUDE_DIR OPENXCOM_LOADER_SDL2_LIBRARY )

  # Function scope prevents _GNU_SOURCE/check includes from escaping into the
  # SDL1 application. These are compile AND link checks, never runtime probes.
  function ( openxcom_check_linux_loader_dependencies )
    include ( CheckCXXSourceCompiles )
    set ( CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE )
    set ( CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE )
    set ( CMAKE_REQUIRED_INCLUDES "" )
    set ( CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} )
    check_cxx_source_compiles ( "
      #include <features.h>
      #include <pthread.h>
      #if !defined(__linux__) || !defined(__GLIBC__)
      #error The loader backend requires Linux and glibc
      #endif
      int (*volatile checked_tryjoin)(pthread_t, void **) = &pthread_tryjoin_np;
      int main() { return checked_tryjoin == nullptr; }
      " OPENXCOM_LOADER_HAS_GLIBC_TRYJOIN )
    if ( NOT OPENXCOM_LOADER_HAS_GLIBC_TRYJOIN )
      message ( FATAL_ERROR "The Linux loader backend requires glibc and declared/linkable pthread_tryjoin_np" )
    endif()

    set ( CMAKE_REQUIRED_DEFINITIONS "" )
    set ( CMAKE_REQUIRED_INCLUDES "${OPENXCOM_LOADER_SDL2_INCLUDE_DIRS}" )
    set ( CMAKE_REQUIRED_LIBRARIES "${OPENXCOM_LOADER_SDL2_LIBRARY}" )
    check_cxx_source_compiles ( "
      #include <SDL_version.h>
      #if SDL_MAJOR_VERSION != 2 || !SDL_VERSION_ATLEAST(2, 0, 16)
      #error The loader cleanup bridge requires SDL2 >= 2.0.16
      #endif
      #include <SDL_thread.h>
      void (SDLCALL *volatile checked_cleanup)(void) = &SDL_TLSCleanup;
      int main() { return checked_cleanup == nullptr; }
      " OPENXCOM_LOADER_HAS_SDL2_TLS_CLEANUP )
    if ( NOT OPENXCOM_LOADER_HAS_SDL2_TLS_CLEANUP )
      message ( FATAL_ERROR "The Linux loader backend requires SDL2 >= 2.0.16 and declared/linkable SDL_TLSCleanup" )
    endif()
  endfunction()
  openxcom_check_linux_loader_dependencies()
endif()

# Find OpenGL
set (OpenGL_GL_PREFERENCE LEGACY)
# The integration never silently enables a dependency or a native profile.
if ( OPENXCOM_BUILD_RESPONSIVE_LOADING )
  if ( NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT OPENXCOM_BUILD_LINUX_LOADER_BACKEND OR
       NOT OPENXCOM_BUILD_LOADING_PRESENTATION_HELPERS OR NOT OPENXCOM_BUILD_LOADING_AUDIO_HANDOFF )
    message ( FATAL_ERROR "Responsive loading requires explicitly enabled Linux/glibc backend, presentation helpers and audio handoff" )
  endif()
  get_cmake_property ( openxcom_responsive_variables VARIABLES )
  foreach ( openxcom_responsive_variable ${openxcom_responsive_variables} )
    if ( openxcom_responsive_variable MATCHES "^EXPERIMENTAL_" AND
         NOT openxcom_responsive_variable STREQUAL "EXPERIMENTAL_MIXED_AUDIO_OBSERVER" )
      if ( ${openxcom_responsive_variable} )
        message ( FATAL_ERROR "Responsive loading is not integrated with ${openxcom_responsive_variable}" )
      endif()
    endif()
  endforeach()
endif()

find_package ( OpenGL )
if ( NOT OPENGL_FOUND )
  message ( WARNING "Can't find OpenGL; how does that even happen?\n"
    "Continuing building without OpenGL support."
    )
  add_definitions(-D__NO_OPENGL)
else ()
    include_directories ( SYSTEM ${OPENGL_INCLUDE_DIR} )
    message ( "found openGL (${OPENGL_LIBRARIES})" )
endif ()

# Resolve backtraces
if ( WIN32 )
	link_libraries( dbghelp )
endif ()
if ( UNIX )
	link_libraries( ${CMAKE_DL_LIBS} )
endif ()

# Read version number
set ( file "${CMAKE_SOURCE_DIR}/src/version.h" )
file ( READ ${file} lines )
string ( REGEX MATCH "[.]*OPENXCOM_VERSION_SHORT \"([0-9]).([0-9])" version_line "${lines}" )
set ( CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1} )
set ( CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2} )
set ( CPACK_PACKAGE_VERSION_PATCH "" )

if ( "${OPENXCOM_VERSION_STRING}" STREQUAL "" )
  find_package ( Git )
  if ( GIT_FOUND )
    message("git found: ${GIT_EXECUTABLE}")
    execute_process ( COMMAND ${GIT_EXECUTABLE} describe --dirty
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE git_describe_out
      ERROR_VARIABLE git_describe_error
      RESULT_VARIABLE git_describe_result
      )
    string ( REGEX MATCH "([a-z|0-9|.]*)-([0-9]*)-g([a-z|0-9]*)([-|a-z]*)" git_commit "${git_describe_out}" )
    set ( git_tag ${CMAKE_MATCH_1} )
    set ( git_nb_commit ${CMAKE_MATCH_2} )
    set ( git_commit ${CMAKE_MATCH_3} )
    set ( git_dirty ${CMAKE_MATCH_4} )
    set ( OPENXCOM_VERSION_STRING ".${git_commit}${git_dirty}" )
  endif()
endif()

add_definitions( -DGIT_BUILD=1 )

configure_file("${CMAKE_SOURCE_DIR}/src/git_version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/git_version.h" )
include_directories ( "${CMAKE_CURRENT_BINARY_DIR}" )


if ( DEV_BUILD )
  # Append the commit to version number
  set ( CPACK_PACKAGE_VERSION_PATCH "${git_commit}${git_dirty}" )
  set ( CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}" )
else ()
  set ( CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_NSIS_PACKAGE_NAME}" )
  set ( CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}" )
endif ()

if ( BUILD_PACKAGE )
  if ( NOT DEV_BUILD )
    string ( LENGTH "${git_dirty}" is_dirty )
    if ( ${is_dirty} GREATER 0 )
      message ( FATAL_ERROR "Release package must be built from a clean tree" )
    endif ()
    if ( "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
      message ( FATAL_ERROR "Release package can't be built from a debug build" )
    endif ()
  endif ( )

  if ( NOT CPACK_GENERATOR )
    set ( CPACK_GENERATOR "TXZ" )
  endif ()
  if ( NOT CPACK_SOURCE_GENERATOR )
    set ( CPACK_SOURCE_GENERATOR "TXZ" )
  endif ()

  set ( CPACK_PACKAGE_VENDOR "The OpenXcom project" )
  set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open-source clone of UFO: Enemy Unknown" )
  set ( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/cmake/modules/Description.txt" )
  set ( CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md" )
  if ( NOT APPLE )
    set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt" )
  endif ()
  set ( CPACK_PACKAGE_CONTACT "The OpenXcom project (http://www.openxcom.org)" )

  include ( LinuxDEB )
  include ( LinuxRPM )
  include ( nsis )
  include ( apple )
  include ( CPack )
  message ( "version:${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}"
)
endif()

if ( NOT WIN32 )
  install(FILES "${CMAKE_SOURCE_DIR}/res/linux/openxcom.desktop"
    DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/applications")
  install(FILES "${CMAKE_SOURCE_DIR}/res/linux/icons/openxcom_48x48.png"
    DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/48x48/apps" RENAME openxcom.png)
  install(FILES "${CMAKE_SOURCE_DIR}/res/linux/icons/openxcom_128x128.png"
    DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME openxcom.png)
  install(FILES "${CMAKE_SOURCE_DIR}/res/linux/icons/openxcom.svg"
    DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/hicolor/scalable/apps")
endif ()

add_subdirectory ( docs )
add_subdirectory ( src )
