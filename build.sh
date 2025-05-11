#!/bin/bash
set -e

# Default values
RUN_FORMAT=false
VERBOSE=false
SHOW_HELP=false
RUN_TESTS=false
BUILD_TYPE="Release"
CLEAN_BUILD=false

# Print usage information
usage() {
    cat <<EOF
Usage: $0 [options]

Build script for the D-Bus Configuration Manager project

Options:
  --format       Run clang-format before building
  -v, --verbose  Show verbose build output
  -h, --help     Show this help message and exit
  -t, --test     Build and run tests
  -d, --debug    Build in debug mode (default: Release)
  -c, --clean    Clean build (removes build directory first)
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --format)
            RUN_FORMAT=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            SHOW_HELP=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        *)
            echo "Error: Unknown option $1"
            usage
            exit 1
            ;;
    esac
done

# Show help if requested
if [ "$SHOW_HELP" = true ]; then
    usage
    exit 0
fi

# Handle clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo "Performing clean build..."
    rm -rf build
fi

# Build directory setup
echo "Setting up build directory..."
mkdir -p build && cd build

# Run CMake configuration
CMAKE_ARGS=(-DCMAKE_INSTALL_PREFIX=../bin -DCMAKE_BUILD_TYPE=$BUILD_TYPE)
if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS+=(-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON)
    echo "Verbose output enabled"
fi

echo "Configuring with build type: $BUILD_TYPE"

cmake "${CMAKE_ARGS[@]}" ..

# Run formatting if requested
if [ "$RUN_FORMAT" = true ]; then
    echo "Running formatting target..."
    cmake --build . --target format
fi

# Build and install
echo "Building and installing..."
cmake --build . --target install --parallel $(nproc)

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    echo "Running tests..."
    cmake --build . --target tests
    ctest --output-on-failure
fi

echo "============================================================"
echo "Build completed successfully."
echo "Executables installed to: $(realpath ../bin)"
echo "To run the demo: ./bin/demo.sh"
echo "============================================================"