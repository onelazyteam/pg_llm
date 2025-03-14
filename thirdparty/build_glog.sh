#!/bin/bash

# Script to download and build glog for pg_llm
# This script will download glog from GitHub and build it

set -e

# Define variables
GLOG_VERSION="v0.6.0"
GLOG_DIR="glog"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
INSTALL_DIR="${SCRIPT_DIR}/install"

# Create directories if they don't exist
mkdir -p "${BUILD_DIR}"
mkdir -p "${INSTALL_DIR}"

# Check if we have git
if ! command -v git &> /dev/null; then
    echo "Error: git is not installed. Please install git first."
    exit 1
fi

# Check if we have cmake
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed. Please install cmake first."
    exit 1
fi

# Clone glog if it doesn't exist
if [ ! -d "${SCRIPT_DIR}/${GLOG_DIR}" ]; then
    echo "Cloning glog repository..."
    git clone --branch ${GLOG_VERSION} --depth 1 git@github.com:google/glog.git "${SCRIPT_DIR}/${GLOG_DIR}"
fi

# Build glog
echo "Building glog..."
cd "${SCRIPT_DIR}/${GLOG_DIR}"

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake .. \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DWITH_GFLAGS=OFF \
    -DWITH_UNWIND=OFF

# Build and install
cmake --build . --config Release --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)"
cmake --install .

echo "glog has been built and installed to ${INSTALL_DIR}"
echo "Include directory: ${INSTALL_DIR}/include"
echo "Library directory: ${INSTALL_DIR}/lib"

# Create a file that indicates successful build
touch "${INSTALL_DIR}/.glog_build_complete" 