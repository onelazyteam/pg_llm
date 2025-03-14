cmake_minimum_required(VERSION 3.15)
project(pg_llm VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type options
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "ASan")

# ASan configuration
if(CMAKE_BUILD_TYPE STREQUAL "ASan")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

# Compiler options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# Build glog dependency
message(STATUS "Setting up glog...")
execute_process(
    COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/build_glog.sh
    RESULT_VARIABLE GLOG_BUILD_RESULT
    OUTPUT_VARIABLE GLOG_BUILD_OUTPUT
    ERROR_VARIABLE GLOG_BUILD_ERROR
)

if (NOT ${GLOG_BUILD_RESULT} EQUAL 0)
    message(FATAL_ERROR "Failed to build glog: ${GLOG_BUILD_ERROR}")
endif()

# Set glog paths
set(GLOG_INSTALL_DIR "${CMAKE_SOURCE_DIR}/thirdparty/install")
set(GLOG_INCLUDE_DIR "${GLOG_INSTALL_DIR}/include")
set(GLOG_LIBRARY_DIR "${GLOG_INSTALL_DIR}/lib")

# OS detection
if(APPLE)
    # macOS specific settings
    message(STATUS "Configuring for macOS")
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE BREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Homebrew prefix: ${BREW_PREFIX}")
    
    # Check if Homebrew is installed
    if(NOT BREW_PREFIX)
        message(FATAL_ERROR "Homebrew is required for macOS build. Please install it from https://brew.sh")
    endif()
    
    # Check required packages
    execute_process(
        COMMAND brew --prefix openssl
        OUTPUT_VARIABLE OPENSSL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix curl
        OUTPUT_VARIABLE CURL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix jsoncpp
        OUTPUT_VARIABLE JSONCPP_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND brew --prefix postgresql
        OUTPUT_VARIABLE POSTGRESQL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    message(STATUS "OpenSSL root dir: ${OPENSSL_ROOT_DIR}")
    message(STATUS "CURL root dir: ${CURL_ROOT_DIR}")
    message(STATUS "JSONCPP root dir: ${JSONCPP_ROOT_DIR}")
    message(STATUS "PostgreSQL root dir: ${POSTGRESQL_ROOT_DIR}")
    
    # Set pkg-config path
    set(ENV{PKG_CONFIG_PATH} "${OPENSSL_ROOT_DIR}/lib/pkgconfig:${CURL_ROOT_DIR}/lib/pkgconfig:${JSONCPP_ROOT_DIR}/lib/pkgconfig:${POSTGRESQL_ROOT_DIR}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    message(STATUS "PKG_CONFIG_PATH: $ENV{PKG_CONFIG_PATH}")
endif()

# PostgreSQL related configuration
find_program(PG_CONFIG pg_config REQUIRED)
execute_process(
    COMMAND ${PG_CONFIG} --includedir
    OUTPUT_VARIABLE PG_INCLUDEDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND ${PG_CONFIG} --includedir-server
    OUTPUT_VARIABLE PG_INCLUDEDIR_SERVER
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

message(STATUS "PostgreSQL include dir: ${PG_INCLUDEDIR}")
message(STATUS "PostgreSQL server include dir: ${PG_INCLUDEDIR_SERVER}")
message(STATUS "PostgreSQL lib dir: ${PG_LIBDIR}")
message(STATUS "PostgreSQL bin dir: ${PG_BINDIR}")
message(STATUS "PostgreSQL pkglib dir: ${PG_PKGLIBDIR}")
message(STATUS "PostgreSQL share dir: ${PG_SHAREDIR}")

# Set PostgreSQL library variable
set(PG_LIBDIR ${PG_LIBDIR} CACHE PATH "PostgreSQL library directory")

# Find dependency libraries
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)

# Special handling for jsoncpp
if(APPLE AND JSONCPP_ROOT_DIR)
    # For macOS, directly set jsoncpp paths
    set(JSONCPP_INCLUDE_DIRS "${JSONCPP_ROOT_DIR}/include")
    find_library(JSONCPP_LIBRARIES NAMES jsoncpp PATHS "${JSONCPP_ROOT_DIR}/lib" NO_DEFAULT_PATH)
    if(NOT JSONCPP_LIBRARIES)
        message(FATAL_ERROR "Cannot find jsoncpp library. Please install it with: brew install jsoncpp")
    endif()
    message(STATUS "Found jsoncpp library: ${JSONCPP_LIBRARIES}")
else()
    # For other platforms, use pkg-config
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
endif()

# Set output directory
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add global include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${PG_INCLUDEDIR}
    ${PG_INCLUDEDIR_SERVER}
    ${GLOG_INCLUDE_DIR}
)

# Add link directories
link_directories(
    ${PG_LIBDIR}
    ${GLOG_LIBRARY_DIR}
)

# Add subdirectories
add_subdirectory(src)

# Install targets
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