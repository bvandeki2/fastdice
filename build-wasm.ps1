# PowerShell build script for WebAssembly using Emscripten

Write-Host "FastDice WebAssembly Build Script" -ForegroundColor Green
Write-Host "==================================" -ForegroundColor Green
Write-Host ""

# Check if emcc is available
$emccPath = Get-Command emcc -ErrorAction SilentlyContinue
if (-not $emccPath) {
    Write-Host "Error: Emscripten (emcc) not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install Emscripten SDK from: https://emscripten.org/docs/getting_started/downloads.html"
    Write-Host ""
    Write-Host "Quick setup on Windows:"
    Write-Host "  git clone https://github.com/emscripten-core/emsdk.git"
    Write-Host "  cd emsdk"
    Write-Host "  .\emsdk install latest"
    Write-Host "  .\emsdk activate latest"
    Write-Host "  .\emsdk_env.ps1"
    Write-Host ""
    exit 1
}

Write-Host "Emscripten version:" -ForegroundColor Cyan
& emcc --version

# Create build directory
$BUILD_DIR = "build-wasm"
if (Test-Path $BUILD_DIR) {
    Write-Host "Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BUILD_DIR
}

New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
Set-Location $BUILD_DIR

# Configure with CMake
Write-Host ""
Write-Host "Configuring CMake..." -ForegroundColor Cyan
& emcmake cmake .. -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

# Build
Write-Host ""
Write-Host "Building..." -ForegroundColor Cyan
& emmake make -j $env:NUMBER_OF_PROCESSORS

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

# Copy output files to web directory
Write-Host ""
Write-Host "Copying output files to web directory..." -ForegroundColor Cyan
Set-Location ..
New-Item -ItemType Directory -Path "web" -Force | Out-Null
Copy-Item "$BUILD_DIR/fastdice.js" -Destination "web/" -Force
Copy-Item "$BUILD_DIR/fastdice.wasm" -Destination "web/" -Force

Write-Host ""
Write-Host "✓ Build complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Output files:" -ForegroundColor Cyan
Write-Host "  - web/fastdice.js"
Write-Host "  - web/fastdice.wasm"
Write-Host "  - web/index.html"
Write-Host ""
Write-Host "To run the demo:" -ForegroundColor Yellow
Write-Host "  cd web"
Write-Host "  python -m http.server 8000"
Write-Host "  # or"
Write-Host "  npx http-server -p 8000"
Write-Host ""
Write-Host "Then open http://localhost:8000 in your browser" -ForegroundColor Yellow
