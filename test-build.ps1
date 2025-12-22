# test-build.ps1 - Build phoenix-waterfall locally

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Host "=== Phoenix Waterfall - Build Test ===" -ForegroundColor Cyan
Write-Host ""

try {
    # Clean build
    Write-Host "[1/3] Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }

    # Configure
    Write-Host "[2/3] Configuring (cmake --preset msys2-ucrt64)..." -ForegroundColor Yellow
    cmake --preset msys2-ucrt64
    if ($LASTEXITCODE -ne 0) { throw "Configure failed" }

    # Build
    Write-Host "[3/3] Building (cmake --build --preset msys2-ucrt64)..." -ForegroundColor Yellow
    cmake --build --preset msys2-ucrt64
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }

    Write-Host ""
    Write-Host "=== BUILD SUCCESSFUL ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "Output:"
    Get-ChildItem "build\msys2-ucrt64\*.exe" | ForEach-Object { Write-Host "  $_" }
}
catch {
    Write-Host ""
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
