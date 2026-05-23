cmake_minimum_required(VERSION 3.15)
project(Divergent CXX)

# may set to an earlier standard, depends on what I do with this
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# TODO:
# change march=native when release build
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -march=native -pthread")

# binary output directly to .div/bin folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/.div/bin)

# system deps (libgit2 is required for plumbing commands)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

include_directories(include ${LIBGIT2_INCLUDE_DIRS})

# source files mapping
file(GLOB_RECURSE SOURCES "src/*.cpp")

# final exec output
add_executable(divergent ${SOURCES})

target_link_libraries(divergent PRIVATE ${LIBGIT2_LIBRARIES} pthread)
