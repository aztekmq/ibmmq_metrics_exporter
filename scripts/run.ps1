<#
.SYNOPSIS
    Run the IBM MQ Metrics Exporter
.PARAMETER Config
    Path to configuration file
.PARAMETER Continuous
    Enable continuous monitoring
.PARAMETER Interval
    Collection interval in seconds
.PARAMETER Port
    Prometheus metrics port
.PARAMETER Verbose
    Enable verbose logging
.EXAMPLE
    .\run.ps1 -Config ..\configs\default.yaml -Continuous
#>

param(
    [string]$Config = "",
    [switch]$Continuous,
    [int]$Interval = 60,
    [int]$Port = 9091,
    [switch]$Verbose,
    [string]$LogLevel = "info",
    [string]$LogFormat = "text"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

# Find binary
$ExeName = "ibmmq-exporter.exe"
$Binary = Get-ChildItem -Path $BuildDir -Recurse -Filter $ExeName -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not $Binary) {
    Write-Host "Binary not found. Run build.ps1 first." -ForegroundColor Red
    exit 1
}

# Build arguments
$RunArgs = @()

if ($Config) {
    $RunArgs += @("-c", $Config)
} else {
    $DefaultConfig = Join-Path $ProjectRoot "configs\default.yaml"
    if (Test-Path $DefaultConfig) {
        $RunArgs += @("-c", $DefaultConfig)
    }
}

if ($Continuous) { $RunArgs += "--continuous" }
if ($Interval -ne 60) { $RunArgs += @("--interval", $Interval) }
if ($Port -ne 9091) { $RunArgs += @("--prometheus-port", $Port) }
if ($Verbose) { $RunArgs += "-v" }
if ($LogLevel -ne "info") { $RunArgs += @("--log-level", $LogLevel) }
if ($LogFormat -ne "json") { $RunArgs += @("--log-format", $LogFormat) }

Write-Host "Starting IBM MQ Metrics Exporter..." -ForegroundColor Cyan
Write-Host "Binary: $($Binary.FullName)" -ForegroundColor DarkGray
Write-Host "Args:   $($RunArgs -join ' ')" -ForegroundColor DarkGray
Write-Host "Metrics: http://localhost:${Port}/metrics" -ForegroundColor Green
Write-Host ""

& $Binary.FullName @RunArgs
