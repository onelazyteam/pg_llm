# Basic configuration
cmake_minimum_required(VERSION 3.15)
project(pg_llm VERSION 1.0.0 LANGUAGES CXX)

# C++ standard settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()

# Compiler options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# Find PostgreSQL
find_program(PG_CONFIG pg_config REQUIRED)
execute_process(
    COMMAND ${PG_CONFIG} --includedir-server
    OUTPUT_VARIABLE PG_INCLUDEDIR_SERVER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND ${PG_CONFIG} --pkglibdir
    OUTPUT_VARIABLE PG_PKGLIBDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND ${PG_CONFIG} --sharedir
    OUTPUT_VARIABLE PG_SHAREDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND ${PG_CONFIG} --libdir
    OUTPUT_VARIABLE PG_LIBDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND ${PG_CONFIG} --bindir
    OUTPUT_VARIABLE PG_BINDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Set pgvector path
set(PGVECTOR_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../pgvector")

# Set thirdparty paths
set(THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/install")

# Find system libraries
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(JsonCpp REQUIRED)

# Set glog paths
set(GLOG_ROOT_DIR "${THIRDPARTY_ROOT}")
set(GLOG_INCLUDE_DIR "${GLOG_ROOT_DIR}/include")
set(GLOG_LIBRARY_DIR "${GLOG_ROOT_DIR}/lib")

# Add subdirectories
add_subdirectory(src)

# Installation configuration
install(
    TARGETS pg_llm
    LIBRARY DESTINATION "${PG_PKGLIBDIR}"
)

install(
    FILES pg_llm.control
    DESTINATION "${PG_SHAREDIR}/extension"
)

install(
    DIRECTORY sql/
    DESTINATION "${PG_SHAREDIR}/extension"
    FILES_MATCHING PATTERN "*.sql"
) 