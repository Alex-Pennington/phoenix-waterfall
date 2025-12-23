# test-ci-build.ps1 - Simulate GitHub Actions build locally
# Run from PowerShell: .\test-ci-build.ps1

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Host "=== Phoenix Waterfall - CI Build Test ===" -ForegroundColor Cyan
Write-Host ""

try {
    # Clean build
    Write-Host "[1/4] Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }

    # Configure
    Write-Host "[2/4] Configuring (cmake --preset msys2-ucrt64)..." -ForegroundColor Yellow
    cmake --preset msys2-ucrt64
    if ($LASTEXITCODE -ne 0) { throw "Configure failed" }

    # Build
    Write-Host "[3/4] Building (cmake --build --preset msys2-ucrt64)..." -ForegroundColor Yellow
    cmake --build --preset msys2-ucrt64
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }

    # Package
    Write-Host "[4/4] Packaging..." -ForegroundColor Yellow
    if (-not (Test-Path "package")) { New-Item -ItemType Directory "package" | Out-Null }
    Copy-Item "build\msys2-ucrt64\waterfall.exe" "package\" -Force
    Copy-Item "build\msys2-ucrt64\*.dll" "package\" -Force -ErrorAction SilentlyContinue
    Copy-Item "README.md" "package\" -Force
    Copy-Item "LICENSE" "package\" -Force

    Write-Host ""
    Write-Host "=== BUILD SUCCESSFUL ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "Package contents:"
    Get-ChildItem "package\*" | ForEach-Object { Write-Host "  $_" }
}
catch {
    Write-Host ""
    Write-Host "=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
