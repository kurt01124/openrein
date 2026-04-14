@echo off
setlocal

:: Move to the project root (one level up from this script)
cd /d "%~dp0.."

echo [openrein] Building wheel...

:: Check for Python
where python >nul 2>&1
if errorlevel 1 (
    echo ERROR: python not found in PATH
    exit /b 1
)

:: Install build dependencies
echo [1/3] Installing build dependencies...
python -m pip install --quiet "scikit-build-core>=0.9" "pybind11>=2.13" build

:: Build the wheel
echo [2/3] Building wheel (CMake + pybind11)...
python -m build --wheel --outdir dist

if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

echo [3/3] Done.
echo.
echo Wheel output:
dir /b dist\*.whl

endlocal
