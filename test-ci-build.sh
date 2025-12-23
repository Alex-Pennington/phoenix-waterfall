#!/bin/bash
# test-ci-build.sh - Simulate GitHub Actions build locally
# Run from bash: ./test-ci-build.sh

set -e

cd "$(dirname "$0")"

echo "=== Phoenix Waterfall - CI Build Test ==="
echo ""

# Step 1: Clean
echo "[1/4] Cleaning build directory..."
rm -rf build

# Step 2: Configure
echo "[2/4] Configuring..."
cmake --preset msys2-ucrt64

# Step 3: Build
echo "[3/4] Building..."
cmake --build --preset msys2-ucrt64

# Step 4: Package
echo "[4/4] Packaging..."
mkdir -p package
cp build/msys2-ucrt64/waterfall.exe package/ 2>/dev/null || true
cp build/msys2-ucrt64/*.dll package/ 2>/dev/null || true
cp README.md package/
cp LICENSE package/

echo ""
echo "=== BUILD SUCCESSFUL ==="
echo ""
echo "Package contents:"
ls -la package/
