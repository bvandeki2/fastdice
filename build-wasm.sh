#!/bin/bash
# Build script for WebAssembly using Emscripten

set -e

echo "FastDice WebAssembly Build Script"
echo "=================================="

# Check if emcc is available
if ! command -v emcc &> /dev/null; then
    echo "Error: Emscripten (emcc) not found!"
    echo "Please install Emscripten SDK from: https://emscripten.org/docs/getting_started/downloads.html"
    echo ""
    echo "Quick setup:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

echo "Emscripten version:"
emcc --version

# Create build directory
BUILD_DIR="build-wasm"
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo ""
echo "Configuring CMake..."
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building..."
emmake make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Copy output files to web directory
echo ""
echo "Copying output files to web directory..."
mkdir -p ../web
cp fastdice.js ../web/
cp fastdice.wasm ../web/

echo ""
echo "✓ Build complete!"
echo ""
echo "Output files:"
echo "  - web/fastdice.js"
echo "  - web/fastdice.wasm"
echo "  - web/index.html"
echo ""
echo "To run the demo:"
echo "  cd web"
echo "  python3 -m http.server 8000"
echo "  # or"
echo "  npx http-server -p 8000"
echo ""
echo "Then open http://localhost:8000 in your browser"
