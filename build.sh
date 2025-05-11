#!/bin/bash
set -e

# Default values
RUN_FORMAT=false
VERBOSE=false
SHOW_HELP=false

# Print usage information
usage() {
    cat <<EOF
Usage: $0 [options]

Build script for the project

Options:
  --format      Run clang-format before building
  -v, --verbose Show verbose build output
  -h, --help    Show this help message and exit
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

# Build directory setup
echo "Setting up build directory..."
mkdir -p build && cd build

# Run CMake configuration
CMAKE_ARGS=(-DCMAKE_INSTALL_PREFIX=../bin)
if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS+=(-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON)
    echo "Verbose output enabled"
fi

cmake "${CMAKE_ARGS[@]}" ..

# Run formatting if requested
if [ "$RUN_FORMAT" = true ]; then
    echo "Running formatting target..."
    cmake --build . --target format
fi

# Build and install
echo "Building and installing..."
cmake --build . --target install --parallel $(nproc)

echo "Build completed successfully."