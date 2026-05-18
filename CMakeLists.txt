cmake_minimum_required(VERSION 3.10)
project(DungeonCrawler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to find system SDL2 first, then fall back to local build
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    # Fall back to locally built SDL2
    set(SDL2_ROOT "/tmp/opencode/sdl2_install")
    find_library(SDL2_LIBRARY SDL2 PATHS "${SDL2_ROOT}/lib" NO_DEFAULT_PATH)
    find_path(SDL2_INCLUDE_DIR SDL.h PATHS "${SDL2_ROOT}/include/SDL2" NO_DEFAULT_PATH)
    if(SDL2_LIBRARY AND SDL2_INCLUDE_DIR)
        set(SDL2_FOUND TRUE)
        set(SDL2_LIBRARIES ${SDL2_LIBRARY})
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
    endif()
endif()

if(NOT SDL2_FOUND)
    message(FATAL_ERROR "SDL2 not found. Install libsdl2-dev or build SDL2 locally.")
endif()

# Game binary (renamed so launcher can find it)
add_executable(dungeon_crawler_game
    src/dungeon_crawler.cpp
    src/sdl_backend.cpp
)

target_include_directories(dungeon_crawler_game PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(dungeon_crawler_game ${SDL2_LIBRARIES})

# Set RPATH so the binary finds libSDL2.so at runtime
set_target_properties(dungeon_crawler_game PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

if(UNIX)
    target_link_libraries(dungeon_crawler_game pthread dl)
endif()

if(MSVC)
    target_compile_options(dungeon_crawler_game PRIVATE /W4)
else()
    target_compile_options(dungeon_crawler_game PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()

# Launcher binary
add_executable(dungeon_crawler
    src/launcher.cpp
)

set_target_properties(dungeon_crawler PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

if(UNIX)
    target_link_libraries(dungeon_crawler pthread dl)
endif()

if(MSVC)
    target_compile_options(dungeon_crawler PRIVATE /W4)
else()
    target_compile_options(dungeon_crawler PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()

install(TARGETS dungeon_crawler_game dungeon_crawler DESTINATION bin)
install(FILES README.md DESTINATION share/dungeon_crawler)
