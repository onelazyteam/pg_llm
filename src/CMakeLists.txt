# Collect all source files
file(GLOB_RECURSE SOURCES
    "*.cpp"
    "models/*.cpp"
    "models/chatgpt/*.cpp"
    "models/deepseek/*.cpp"
    "models/hunyuan/*.cpp"
    "models/qianwen/*.cpp"
)

# Create shared library target
add_library(pg_llm MODULE ${SOURCES})

# Set target properties
set_target_properties(pg_llm PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pg_llm"
)

# Add include directories
target_include_directories(pg_llm PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

# Link dependent libraries
target_link_libraries(pg_llm PRIVATE
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    -L${PG_LIBDIR} -lpq -lpgcommon -lpgport
)

# PostgreSQL link options - for macOS
if(APPLE)
    target_link_options(pg_llm PRIVATE
        -bundle_loader "${PG_BINDIR}/postgres"
    )
endif()

# Add debug information
message(STATUS "Source files: ${SOURCES}")
message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "CURL include dirs: ${CURL_INCLUDE_DIRS}")
message(STATUS "JSONCPP include dirs: ${JSONCPP_INCLUDE_DIRS}")
message(STATUS "JSONCPP libraries: ${JSONCPP_LIBRARIES}")
message(STATUS "PostgreSQL lib dir: ${PG_LIBDIR}")
message(STATUS "PostgreSQL bin dir: ${PG_BINDIR}") 