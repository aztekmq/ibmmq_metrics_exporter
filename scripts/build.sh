#!/usr/bin/env bash
# Build script for IBM MQ Metrics Exporter (C++)
#
# Usage:
#   ./build.sh                    # Default: gcc, Release, stub MQ off
#   ./build.sh --compiler clang   # Use Clang
#   ./build.sh --stub-mq --clean  # Stub MQ, clean build
#   ./build.sh --debug            # Debug build
#   ./build.sh --mq-home /opt/mqm # Set MQ installation path

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

# Defaults
COMPILER="gcc"
BUILD_TYPE="Release"
STUB_MQ="OFF"
CLEAN=false
MQ_HOME=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --compiler)   COMPILER="$2"; shift 2 ;;
        --debug)      BUILD_TYPE="Debug"; shift ;;
        --release)    BUILD_TYPE="Release"; shift ;;
        --stub-mq)    STUB_MQ="ON"; shift ;;
        --clean)      CLEAN=true; shift ;;
        --mq-home)    MQ_HOME="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "  --compiler gcc|clang   Compiler (default: gcc)"
            echo "  --debug                Debug build"
            echo "  --release              Release build (default)"
            echo "  --stub-mq             Use stub MQ headers"
            echo "  --clean                Clean before building"
            echo "  --mq-home PATH         IBM MQ installation path"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "=== IBM MQ Metrics Exporter - C++ Build ==="
echo "Compiler:   ${COMPILER}"
echo "Build Type: ${BUILD_TYPE}"
echo "Stub MQ:    ${STUB_MQ}"
echo ""

# Clean
if [[ "$CLEAN" == true ]] && [[ -d "$BUILD_DIR" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# CMake arguments
CMAKE_ARGS=(
    -S "$PROJECT_ROOT"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DIBMMQ_EXPORTER_USE_STUB_MQ="$STUB_MQ"
)

if [[ -n "$MQ_HOME" ]]; then
    CMAKE_ARGS+=(-DMQ_HOME="$MQ_HOME")
fi

# Compiler selection
case "$COMPILER" in
    gcc)
        CMAKE_ARGS+=(
            -DCMAKE_C_COMPILER=gcc
            -DCMAKE_CXX_COMPILER=g++
        )
        ;;
    clang)
        CMAKE_ARGS+=(
            -DCMAKE_C_COMPILER=clang
            -DCMAKE_CXX_COMPILER=clang++
        )
        ;;
    *)
        echo "Unknown compiler: ${COMPILER}"
        exit 1
        ;;
esac

# Prefer Ninja if available
if command -v ninja &>/dev/null; then
    CMAKE_ARGS+=(-G Ninja)
fi

# Configure
echo "Configuring with CMake..."
cmake "${CMAKE_ARGS[@]}"

# Build
echo ""
echo "Building..."
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$(nproc 2>/dev/null || echo 4)"

# Report
BINARY="${BUILD_DIR}/ibmmq-exporter"
if [[ -f "$BINARY" ]]; then
    echo ""
    echo "Build successful!"
    echo "Binary: ${BINARY}"
    ls -lh "$BINARY"
else
    echo ""
    echo "Build completed. Check ${BUILD_DIR} for output."
fi
