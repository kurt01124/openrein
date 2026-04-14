#!/usr/bin/env bash
set -euo pipefail

# Move to the project root (one level up from this script)
cd "$(dirname "$0")/.."

echo "[openrein] Building wheel..."

# Check for Python
if ! command -v python3 &>/dev/null; then
    echo "ERROR: python3 not found in PATH"
    exit 1
fi

PYTHON=python3

# Install build dependencies
echo "[1/3] Installing build dependencies..."
$PYTHON -m pip install --quiet "scikit-build-core>=0.9" "pybind11>=2.13" build

# Build the wheel
echo "[2/3] Building wheel (CMake + pybind11)..."
$PYTHON -m build --wheel --outdir dist

echo "[3/3] Done."
echo ""
echo "Wheel output:"
ls dist/*.whl
