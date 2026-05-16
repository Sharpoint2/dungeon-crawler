cmake_minimum_required(VERSION 3.10)
project(DungeonCrawler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dungeon_crawler src/dungeon_crawler.cpp)

if(UNIX)
    target_link_libraries(dungeon_crawler pthread)
endif()

if(MSVC)
    target_compile_options(dungeon_crawler PRIVATE /W4)
else()
    target_compile_options(dungeon_crawler PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()

install(TARGETS dungeon_crawler DESTINATION bin)
install(FILES README.md DESTINATION share/dungeon_crawler)
