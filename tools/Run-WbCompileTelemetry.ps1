# Workbench compile telemetry launcher (Windows PowerShell)
# Usage:
#   1. Close Workbench (optional but cleaner for --wait-new-session)
#   2. Run this script
#   3. Start Workbench / open RSS project and let it compile
#   4. On crash (or Ctrl+C), report is written under logs\telemetry\

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$TelemetryPy = Join-Path $ScriptDir "wb_compile_telemetry.py"
$LogsRoot = Join-Path $env:USERPROFILE "Documents\My Games\ArmaReforgerWorkbench\logs"
$OutDir = Join-Path $LogsRoot "telemetry"

Write-Host "RSS Workbench compile telemetry"
Write-Host "  script : $TelemetryPy"
Write-Host "  logs   : $LogsRoot"
Write-Host "  out    : $OutDir"
Write-Host ""

# Ensure psutil for memory samples (best-effort)
python -c "import psutil" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Installing psutil..."
    python -m pip install --user psutil
}

Write-Host "Starting monitor: waiting for Workbench + new log session..."
Write-Host "Start Workbench now if it is not running."
Write-Host ""

python $TelemetryPy `
    --logs-root $LogsRoot `
    --out-dir $OutDir `
    --wait-process `
    --wait-new-session `
    --timeout-sec 900 `
    --poll-ms 150 `
    --sample-ms 400

$exitCode = $LASTEXITCODE
Write-Host ""
Write-Host "Monitor exit code: $exitCode"
if (Test-Path $OutDir) {
    $latest = Get-ChildItem $OutDir -Filter "wb_telemetry_*.md" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($latest) {
        Write-Host "Latest report: $($latest.FullName)"
        Write-Host "----- report preview -----"
        Get-Content $latest.FullName -TotalCount 40
    }
}

exit $exitCode
