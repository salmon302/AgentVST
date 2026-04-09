Param(
    [Parameter(Mandatory=$false)]
    [string]$PluginValPath = "C:\Users\salmo\Documents\pluginval.exe",
    [Parameter(Mandatory=$false)]
    [string]$PluginsFolder,
    [Parameter(Mandatory=$false)]
    [string]$LogFolder
)

$WorkspaceRoot = Resolve-Path "$PSScriptRoot\.." | Select-Object -ExpandProperty Path
if (-not $PluginsFolder) { $PluginsFolder = Join-Path $WorkspaceRoot "deployed_vst3" }
if (-not $LogFolder) { $LogFolder = Join-Path $WorkspaceRoot "build\pluginval_logs" }

if (-not (Test-Path $PluginValPath)) {
    Write-Error "pluginval not found at $PluginValPath. Pass -PluginValPath with correct path."
    exit 2
}

if (-not (Test-Path $PluginsFolder)) {
    Write-Error "Plugins folder not found: $PluginsFolder"
    exit 3
}

New-Item -ItemType Directory -Force -Path $LogFolder | Out-Null

$plugins = Get-ChildItem -Path $PluginsFolder -Directory | Where-Object { $_.Name -match '\.vst3$' }
if ($plugins.Count -eq 0) {
    Write-Error "No .vst3 plugin directories found in $PluginsFolder"
    exit 4
}

$summary = @()
foreach ($p in $plugins) {
    $pluginPath = $p.FullName
    $logFile = Join-Path $LogFolder ("$($p.Name).log")
    Write-Host ("Running pluginval on {0} -> {1}" -f $pluginPath, $logFile)
    & $PluginValPath $pluginPath 2>&1 | Tee-Object -FilePath $logFile
    $rc = $LASTEXITCODE
    $summary += [PSCustomObject]@{ Plugin = $p.Name; Path = $pluginPath; ExitCode = $rc; Log = $logFile }
}

Write-Host "`nSummary:"
$summary | Format-Table -AutoSize

$failed = $summary | Where-Object { $_.ExitCode -ne 0 }
if ($failed) {
    Write-Host "`nFailures:"
    $failed | Format-Table -AutoSize
    exit 1
}
else {
    Write-Host "`nAll pluginval runs exited with code 0."
    exit 0
}
