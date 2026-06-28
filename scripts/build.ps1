<#
.SYNOPSIS
    Build script for IBM MQ Metrics Exporter (C++)
.DESCRIPTION
    Configures and builds the project using CMake with GCC (MinGW), Clang, or MSVC.
.PARAMETER Compiler
    Compiler to use: gcc, clang, or msvc (default: gcc)
.PARAMETER BuildType
    Build type: Debug or Release (default: Release)
.PARAMETER StubMQ
    Build with stub MQ headers (no real IBM MQ dependency)
.PARAMETER Clean
    Clean build directory before building
.PARAMETER MQHome
    Path to IBM MQ installation
.EXAMPLE
    .\build.ps1
    .\build.ps1 -Compiler msvc -BuildType Debug
    .\build.ps1 -StubMQ -Clean
#>

param(
    [ValidateSet("gcc", "clang", "msvc")]
    [string]$Compiler = "gcc",

    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$BuildType = "Release",

    [switch]$StubMQ,
    [switch]$Clean,
    [string]$MQHome = "",
    [string]$GCCPath = "C:\mingw64\mingw64"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host "=== IBM MQ Metrics Exporter - C++ Build ===" -ForegroundColor Cyan
Write-Host "Compiler:   $Compiler"
Write-Host "Build Type: $BuildType"
Write-Host "Stub MQ:    $StubMQ"
Write-Host ""

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Build CMake arguments
$CMakeArgs = @(
    "-S", $ProjectRoot,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

if ($StubMQ) {
    $CMakeArgs += "-DIBMMQ_EXPORTER_USE_STUB_MQ=ON"
}

if ($MQHome) {
    $CMakeArgs += "-DMQ_HOME=$MQHome"
}

# Compiler-specific configuration
switch ($Compiler) {
    "gcc" {
        $GCC = Join-Path $GCCPath "bin\gcc.exe"
        $GXX = Join-Path $GCCPath "bin\g++.exe"
        $Make = Join-Path $GCCPath "bin\mingw32-make.exe"

        if (-not (Test-Path $GXX)) {
            Write-Host "ERROR: GCC not found at $GCCPath" -ForegroundColor Red
            Write-Host "Set -GCCPath to your MinGW installation" -ForegroundColor Red
            exit 1
        }

        Write-Host "Using GCC: $GXX" -ForegroundColor Green
        $CMakeArgs += @(
            "-G", "MinGW Makefiles",
            "-DCMAKE_C_COMPILER=$GCC",
            "-DCMAKE_CXX_COMPILER=$GXX",
            "-DCMAKE_MAKE_PROGRAM=$Make"
        )
    }
    "clang" {
        $CMakeArgs += @(
            "-G", "Ninja",
            "-DCMAKE_C_COMPILER=clang",
            "-DCMAKE_CXX_COMPILER=clang++"
        )
    }
    "msvc" {
        # Auto-detect Visual Studio version via vswhere
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $generator = $null
        if (Test-Path $vsWhere) {
            $vsVersion = & $vsWhere -latest -property catalog_productLineVersion 2>$null
            $vsMajor = & $vsWhere -latest -property installationVersion 2>$null
            if ($vsMajor) {
                $majorNum = ($vsMajor -split '\.')[0]
                # Only map VS versions with known CMake generator support.
                # Newer VS versions (18+) may not have a matching CMake generator yet.
                $vsGenerators = @{
                    "17" = "Visual Studio 17 2022"
                }
                if ($vsGenerators.ContainsKey($majorNum)) {
                    $generator = $vsGenerators[$majorNum]
                }
                Write-Host "Detected Visual Studio $vsVersion (v$vsMajor)" -ForegroundColor Green
            }
        }

        if ($generator) {
            $CMakeArgs += @("-G", $generator, "-A", "x64")
        } else {
            # Fall back to Ninja for VS versions without a CMake generator.
            # Ninja works with any MSVC toolchain and is widely available.
            Write-Host "Using Ninja generator (no CMake generator for this VS version)" -ForegroundColor Yellow
            $CMakeArgs += @("-G", "Ninja")
        }
    }
}

# Configure
Write-Host "`nConfiguring with CMake..." -ForegroundColor Cyan
Write-Host "cmake $($CMakeArgs -join ' ')" -ForegroundColor DarkGray
& cmake @CMakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "`nBuilding..." -ForegroundColor Cyan
$BuildArgs = @("--build", $BuildDir, "--config", $BuildType, "--parallel")
& cmake @BuildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

# Find the built binary
$ExeName = "ibmmq-exporter.exe"
$BinaryPath = Get-ChildItem -Path $BuildDir -Recurse -Filter $ExeName -ErrorAction SilentlyContinue | Select-Object -First 1

if ($BinaryPath) {
    Write-Host "`nBuild successful!" -ForegroundColor Green
    Write-Host "Binary: $($BinaryPath.FullName)" -ForegroundColor Green
} else {
    Write-Host "`nBuild completed but binary not found in $BuildDir" -ForegroundColor Yellow
}
