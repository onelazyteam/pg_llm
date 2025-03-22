# Source files list
set(SOURCES
    pg_llm.cpp
    catalog/pg_llm_models.cpp
    models/model_manager.cpp
    models/llm_interface.cpp
    models/chat_session.cpp
    models/chatgpt/chatgpt_model.cpp
    models/deepseek/deepseek_model.cpp
    models/hunyuan/hunyuan_model.cpp
    models/qianwen/qianwen_model.cpp
    utils/pg_llm_glog.cpp
)

add_compile_options(-Wall -Wextra -Werror -Wunused-variable)

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