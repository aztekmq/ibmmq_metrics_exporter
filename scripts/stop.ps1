#!/usr/bin/env pwsh
# Stop the IBM MQ Metrics Exporter

Write-Host "Stopping IBM MQ Metrics Exporter..."

# Find and kill any running exporter processes
$processes = Get-Process ibmmq-exporter -ErrorAction SilentlyContinue
if ($processes) {
    $processes | Stop-Process -Force
    Write-Host "Exporter processes stopped."
} else {
    Write-Host "No running exporter processes found."
}

Write-Host "Cleanup complete."
