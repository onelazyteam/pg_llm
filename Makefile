MODULE_big = pg_llm
EXTENSION = pg_llm
PGFILEDESC = "pg_llm - PostgreSQL LLM Extension"

# Build type (debug/release)
BUILD_TYPE ?= release

# Output directory
BUILD_DIR = build/$(BUILD_TYPE)

# Automatically find all source files
SRCS = $(wildcard src/models/*.cpp) \
       $(wildcard src/models/chatgpt/*.cpp) \
       $(wildcard src/models/deepseek/*.cpp) \
       $(wildcard src/models/hunyuan/*.cpp) \
       $(wildcard src/models/qianwen/*.cpp) \
       $(wildcard src/pg_llm.cpp)

# Generate object files in build directory
OBJS = $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

# Create object file directories
$(shell mkdir -p $(sort $(dir $(OBJS))))

# Detect OS type
UNAME_S := $(shell uname -s)

# Common flags and paths
PG_CPPFLAGS = -I$(shell $(PG_CONFIG) --includedir) \
              -I$(CURDIR)/include \
              -std=c++17

# Build type specific flags
ifeq ($(BUILD_TYPE),debug)
    PG_CPPFLAGS += -g -O0 -DDEBUG \
                   -Wall -Wextra -Werror \
                   -fno-omit-frame-pointer \
                   -fsanitize=address
    SHLIB_LINK += -fsanitize=address
else
    PG_CPPFLAGS += -O2 -DNDEBUG
endif

SHLIB_LINK = -lstdc++ -lcurl -lcrypto -lssl -ljsoncpp

# OS-specific configurations
ifeq ($(UNAME_S),Darwin)
    # macOS specific settings
    BREW_PREFIX := $(shell brew --prefix)
    
    # Check if Homebrew is installed
    ifeq ($(BREW_PREFIX),)
        $(error Homebrew is required for macOS build. Please install it from https://brew.sh)
    endif
    
    # Check for required packages
    ifneq ($(shell brew ls --versions openssl),)
        OPENSSL_PREFIX := $(shell brew --prefix openssl)
        PG_CPPFLAGS += -I$(OPENSSL_PREFIX)/include
        SHLIB_LINK += -L$(OPENSSL_PREFIX)/lib
    else
        $(error OpenSSL is required. Please install it with: brew install openssl)
    endif
    
    ifneq ($(shell brew ls --versions jsoncpp),)
        JSONCPP_PREFIX := $(shell brew --prefix jsoncpp)
        PG_CPPFLAGS += -I$(JSONCPP_PREFIX)/include
        SHLIB_LINK += -L$(JSONCPP_PREFIX)/lib
    else
        $(error JsonCPP is required. Please install it with: brew install jsoncpp)
    endif
    
    # macOS specific linker flags
    SHLIB_LINK += -Wl,-rpath,$(OPENSSL_PREFIX)/lib
    SHLIB_LINK += -Wl,-rpath,$(JSONCPP_PREFIX)/lib
else
    # Linux specific settings
    
    # Check for required packages using pkg-config
    ifneq ($(shell which pkg-config),)
        # OpenSSL
        ifeq ($(shell pkg-config --exists openssl && echo yes),yes)
            PG_CPPFLAGS += $(shell pkg-config --cflags openssl)
            SHLIB_LINK += $(shell pkg-config --libs openssl)
        else
            $(error OpenSSL development files not found. Please install libssl-dev)
        endif
        
        # JsonCPP
        ifeq ($(shell pkg-config --exists jsoncpp && echo yes),yes)
            PG_CPPFLAGS += $(shell pkg-config --cflags jsoncpp)
            SHLIB_LINK += $(shell pkg-config --libs jsoncpp)
        else
            $(error JsonCPP development files not found. Please install libjsoncpp-dev)
        endif
    else
        $(error pkg-config is required for Linux build)
    endif
    
    # Add common Linux library paths
    PG_CPPFLAGS += -I/usr/include
    SHLIB_LINK += -L/usr/lib
endif

# PostgreSQL extension SQL files
DATA = $(wildcard sql/*.sql)
REGRESS = $(patsubst test/sql/%.sql,%,$(wildcard test/sql/*.sql))

# Use PostgreSQL PGXS build system
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

# Custom compilation rule for C++ files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(PG_CPPFLAGS) $(CPPFLAGS) -c -o $@ $<

# Phony targets
.PHONY: all clean clean-all install check-deps debug release

# Build targets
debug:
	$(MAKE) BUILD_TYPE=debug

release:
	$(MAKE) BUILD_TYPE=release

# Dependencies check target
check-deps:
	@echo "Checking build dependencies..."
	@echo "Build type: $(BUILD_TYPE)"
ifeq ($(UNAME_S),Darwin)
	@which brew > /dev/null || (echo "Error: Homebrew is required" && exit 1)
	@brew ls --versions openssl > /dev/null || (echo "Error: OpenSSL is required" && exit 1)
	@brew ls --versions jsoncpp > /dev/null || (echo "Error: JsonCPP is required" && exit 1)
else
	@which pkg-config > /dev/null || (echo "Error: pkg-config is required" && exit 1)
	@pkg-config --exists openssl || (echo "Error: OpenSSL development files are required" && exit 1)
	@pkg-config --exists jsoncpp || (echo "Error: JsonCPP development files are required" && exit 1)
endif
	@echo "All dependencies are satisfied"

# Make all target now depends on check-deps
all: check-deps

# Cleaning targets
clean:
	rm -rf build/

clean-all: clean
	rm -f *.so *.dylib  # Remove shared library files for both platforms

# Install targets
install:
	$(INSTALL_PROGRAM) $(BUILD_DIR)/$(MODULE_big).so $(DESTDIR)$(pgxs_libdir)/$(MODULE_big).so
	$(INSTALL_DATA) $(DATA) $(DESTDIR)$(datadir)/$(MODULE_big)
	$(INSTALL_DATA) $(REGRESS) $(DESTDIR)$(datadir)/$(MODULE_big)
