cmake_minimum_required(VERSION 3.16)
project(RebirthOfCivilisation VERSION 0.26.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Static MSVC runtime so the .exe runs on any Windows PC without redistributables
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(REBIRTH_BUILD_TESTS "Build the headless simulation and screenshot tools" ON)

# ---- portable game: rules engine + user interface (draws through the Canvas interface)
add_library(rebirth_game STATIC
  src/core/Data.cpp
  src/core/Game.cpp
  src/core/MapGen.cpp
  src/core/Turn.cpp
  src/core/AI.cpp
  src/core/Disasters.cpp
  src/core/Bands.cpp
  src/core/Nukes.cpp
  src/core/Naval.cpp
  src/core/Transport.cpp
  src/core/Diplomacy.cpp
  src/core/Trade.cpp
  src/core/Rivers.cpp
  src/core/Happiness.cpp
  src/core/Tribes.cpp
  src/core/Great.cpp
  src/core/Religion.cpp
  src/core/Council.cpp
  src/core/Promotions.cpp
  src/core/Achievements.cpp
  src/core/Save.cpp
  src/ui/GameUI.cpp
  src/ui/UIMap.cpp
  src/ui/UIArt.cpp
  src/ui/UITrade.cpp
  src/ui/UIRivers.cpp
  src/ui/UIResources.cpp
  src/ui/UITutorial.cpp
  src/ui/UIAdvisors.cpp
  src/ui/UIEmpire.cpp
  src/ui/UIReligion.cpp
  src/ui/UICouncil.cpp
  src/ui/UISound.cpp
)
target_include_directories(rebirth_game PUBLIC src)
if(MSVC)
  target_compile_options(rebirth_game PRIVATE /W3 /utf-8 /permissive-)
else()
  target_compile_options(rebirth_game PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# ---- audio: a portable software mixer and the stb_vorbis decoder (public domain)
add_library(rebirth_audio STATIC
  src/audio/Mixer.cpp
  third_party/stb_vorbis.c
)
target_include_directories(rebirth_audio PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(rebirth_audio PUBLIC Threads::Threads)
if(MSVC)
  set_source_files_properties(third_party/stb_vorbis.c PROPERTIES COMPILE_OPTIONS "/w")
  target_compile_options(rebirth_audio PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W3 /utf-8>)
else()
  set_source_files_properties(third_party/stb_vorbis.c PROPERTIES COMPILE_OPTIONS "-w")
endif()

# ---- the Windows game (native Win32 + GDI)
if(WIN32)
  enable_language(RC)
  # one version for everything: the .exe's product/file version comes from project(VERSION ...)
  set(REBIRTH_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
  configure_file(res/rebirth.rc.in "${CMAKE_CURRENT_BINARY_DIR}/rebirth.rc" @ONLY)
  add_executable(RebirthOfCivilisation WIN32
    src/platform/win32/main_win32.cpp
    src/platform/win32/audio_win32.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/rebirth.rc"
  )
  target_link_libraries(RebirthOfCivilisation PRIVATE rebirth_game rebirth_audio gdiplus gdi32 user32 ole32 winmm)
  target_compile_definitions(RebirthOfCivilisation PRIVATE _WIN32_WINNT=0x0601 WINVER=0x0601)
  if(MSVC)
    target_compile_options(RebirthOfCivilisation PRIVATE /W3 /utf-8 /permissive-)
  else()
    target_link_options(RebirthOfCivilisation PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
endif()

# ---- tests (run anywhere)
if(REBIRTH_BUILD_TESTS)
  enable_testing()
  add_executable(rebirth_sim tests/sim.cpp)
  target_link_libraries(rebirth_sim PRIVATE rebirth_game)
  add_executable(rebirth_screens tests/screens.cpp)
  target_link_libraries(rebirth_screens PRIVATE rebirth_game)
  add_test(NAME ai_simulation_small_quick COMMAND rebirth_sim 2 0 0)
  add_test(NAME ai_simulation_standard_map_standard_speed COMMAND rebirth_sim 1 1 1)
  add_executable(rebirth_audio_test tests/audio.cpp)
  target_link_libraries(rebirth_audio_test PRIVATE rebirth_audio)
  add_test(NAME audio_mixer COMMAND rebirth_audio_test "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/")
endif()
