cmake_minimum_required(VERSION 3.20)

project(Diablo1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 REQUIRED)

add_executable(diablo1
    src/main.cpp
    src/audio/music.cpp
    src/font.cpp
    src/states/intro_state.cpp
    src/states/main_menu_state.cpp
    src/states/new_hero_state.cpp
    src/states/select_hero_state.cpp
    src/states/playing_state.cpp
    src/smacker/libsmackerdec/BitReader.cpp
    src/smacker/libsmackerdec/FileStream.cpp
    src/smacker/libsmackerdec/LogError.cpp
    src/smacker/libsmackerdec/SmackerDecoder.cpp
    src/game.cpp
    src/smacker/libsmackker.cpp
    src/pcx.cpp
    src/smacker/pklib/explode.c
    src/audio/sfx.cpp
    src/storm/stormlib.cpp
    src/video.cpp
)

target_compile_options(diablo1 PRIVATE -Wall -Wextra -pedantic)
target_compile_definitions(diablo1 PRIVATE USE_SDL3=1)
target_include_directories(diablo1 PRIVATE src src/smacker/libsmackerdec)
target_link_libraries(diablo1 PRIVATE SDL3::SDL3)