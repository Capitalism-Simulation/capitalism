cmake_minimum_required(VERSION 3.16)
project(Capitalism CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Capitalism
    Source/main.cpp
    Source/Game.cpp
    Source/GameData.cpp
    Source/Console.cpp
    Source/Json.cpp
)
target_include_directories(Capitalism PRIVATE Header)

if(MSVC)
    target_compile_options(Capitalism PRIVATE /utf-8 /W4)
else()
    target_compile_options(Capitalism PRIVATE -Wall -Wextra)
endif()

# Copy game data next to the executable so it runs from the build folder.
add_custom_command(TARGET Capitalism POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:Capitalism>/data)
