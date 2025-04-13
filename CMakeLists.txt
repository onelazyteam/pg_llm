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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wunused-variable")
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
set(PGVECTOR_INCLUDE_DIR "${PG_INCLUDEDIR_SERVER}/extension/vector")

# Set thirdparty paths
set(THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/install")
set(GLOG_INCLUDE_DIR "${THIRDPARTY_ROOT}/include")
set(GLOG_LIBRARY_DIR "${THIRDPARTY_ROOT}/lib")

# Find system libraries
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(JsonCpp REQUIRED)

# Source files list
set(SOURCES
    src/pg_llm.cpp
    src/catalog/pg_llm_models.cpp
    src/models/model_manager.cpp
    src/models/llm_interface.cpp
    src/utils/pg_llm_glog.cpp
    src/text2sql/pg_vector.cpp
    src/text2sql/text2sql.cpp
)

# Create shared library
add_library(pg_llm MODULE ${SOURCES})

# Set target properties
set_target_properties(pg_llm PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pg_llm"
)

# Platform specific settings
if(APPLE)
    set_target_properties(pg_llm PROPERTIES SUFFIX ".dylib")
    target_link_options(pg_llm PRIVATE
        -bundle_loader "${PG_BINDIR}/postgres"
    )
else()
    set_target_properties(pg_llm PROPERTIES SUFFIX ".so")
endif()

# Include directories
target_include_directories(pg_llm PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${PG_INCLUDEDIR_SERVER}
    ${PGVECTOR_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIR}
    ${JsonCpp_INCLUDE_DIR}
    ${GLOG_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(pg_llm PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    JsonCpp::JsonCpp
    -L${GLOG_LIBRARY_DIR} -lglog
    -L${PG_LIBDIR} -lpq -lpgcommon -lpgport
)

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