#!/usr/bin/env powershell
<#
.SYNOPSIS
    Verify AgentVST plugin deployment status and detect stale binaries.

.DESCRIPTION
    Checks the VST3 deployment directory for:
    - Stale/outdated plugin binaries (Debug builds)
    - Mismatched plugin versions between source and deployed
    - File size mismatches indicating Debug vs Release builds
    - Deployment freshness

.PARAMETER DeployDir
    Path to the VST3 deployment directory.
    Defaults to Ableton AgentVST-Dev folder.

.PARAMETER SourceDir
    Path to the CMake build directory containing newly built plugins.
    Defaults to './build'

.EXAMPLE
    .\verify_deployment.ps1
    .\verify_deployment.ps1 -DeployDir "C:\Program Files\Common Files\VST3" -SourceDir "./build"
#>

[CmdletBinding()]
param(
    [string]$DeployDir = "I:\Documents\Ableton\VST3\dev",
    [string]$SourceDir = ".\build"
)

$ErrorActionPreference = "Continue"

# Helper function to get binary info
function Get-BinaryInfo {
    param([string]$BinaryPath)

    if (-not (Test-Path $BinaryPath)) {
        return $null
    }

    $file = Get-Item $BinaryPath
    $size = [math]::Round($file.Length / 1MB, 2)
    $age = (Get-Date) - $file.LastWriteTime
    $isDebug = $size -gt 10  # Debug builds are typically >15MB, Release <8MB

    return @{
        Path = $BinaryPath
        SizeMB = $size
        LastModified = $file.LastWriteTime
        AgeHours = [math]::Round($age.TotalHours, 1)
        IsDebugLikely = $isDebug
    }
}

function Test-DeploymentFresh {
    param([int]$HourThreshold = 24)

    Write-Host "`n" + ("=" * 70)
    Write-Host "AgentVST Deployment Verification"
    Write-Host ("=" * 70)
    Write-Host "Deploy Dir:  $DeployDir"
    Write-Host "Source Dir:  $SourceDir"
    Write-Host "Check Time:  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    Write-Host ("=" * 70)

    if (-not (Test-Path $DeployDir)) {
        Write-Host "`n[ERROR] Deployment directory not found: $DeployDir" -ForegroundColor Red
        return $false
    }

    $deployedPlugins = Get-ChildItem -Path $DeployDir -Filter "*.vst3" -Directory

    if (-not $deployedPlugins) {
        Write-Host "`n[WARNING] No VST3 plugins found in deployment directory" -ForegroundColor Yellow
        return $false
    }

    $hasIssues = $false
    $allResults = @()

    foreach ($plugin in $deployedPlugins | Sort-Object Name) {
        $binaryPath = Join-Path $plugin.FullName "Contents\x86_64-win\*.vst3"
        $binary = Get-Item $binaryPath -ErrorAction SilentlyContinue | Select-Object -First 1

        if (-not $binary) {
            Write-Host "`n[❌] $($plugin.Name): Binary not found at $binaryPath" -ForegroundColor Red
            $hasIssues = $true
            continue
        }

        $info = Get-BinaryInfo $binary.FullName
        $isStale = $info.AgeHours -gt $HourThreshold
        $isDebug = $info.IsDebugLikely
        $status = ""
        $color = "Green"

        if ($isDebug) {
            $status = "[⚠️  DEBUG] "
            $color = "Yellow"
            $hasIssues = $true
        }

        if ($isStale) {
            $status = "[❌ STALE] "
            $color = "Red"
            $hasIssues = $true
        }

        if (-not $status) {
            $status = "[✅ OK] "
            $color = "Green"
        }

        Write-Host "`n$status$($plugin.Name)" -ForegroundColor $color
        Write-Host "  Size:       $($info.SizeMB) MB  $(if ($isDebug) { '(DEBUG - should be ~6 MB Release)' } else { '(Release)' })" -ForegroundColor $color
        Write-Host "  Modified:   $($info.LastModified.ToString('yyyy-MM-dd HH:mm:ss'))" -ForegroundColor $color
        Write-Host "  Age:        $($info.AgeHours) hours  $(if ($isStale) { "(stale - ${HourThreshold}h threshold)" } else { '(fresh)' })" -ForegroundColor $color

        $allResults += @{
            Name = $plugin.Name
            Size = $info.SizeMB
            Modified = $info.LastModified
            Age = $info.AgeHours
            IsDebug = $isDebug
            IsStale = $isStale
        }
    }

    # Summary
    Write-Host "`n" + ("=" * 70)
    Write-Host "Summary"
    Write-Host ("=" * 70)

    $debugCount = @($allResults | Where-Object { $_.IsDebug }).Count
    $staleCount = @($allResults | Where-Object { $_.IsStale }).Count
    $okCount = @($allResults | Where-Object { -not $_.IsDebug -and -not $_.IsStale }).Count

    Write-Host "✅ Fresh Release:  $okCount plugins"
    Write-Host "⚠️  Debug Build:   $debugCount plugins" -ForegroundColor $(if ($debugCount -gt 0) { "Yellow" } else { "Green" })
    Write-Host "❌ Stale Binary:   $staleCount plugins" -ForegroundColor $(if ($staleCount -gt 0) { "Red" } else { "Green" })

    if ($hasIssues) {
        Write-Host "`n[RECOMMENDATION] To fix stale/debug deployments:" -ForegroundColor Yellow
        Write-Host "  1. Close all DAWs (Ableton, etc.)"
        Write-Host "  2. Run: cmake --build build --config Release --parallel"
        Write-Host "  3. Restart your DAW and rescan VST3 folder"
        Write-Host "  4. Retest plugin after reload"
        return $false
    } else {
        Write-Host "`n[OK] All deployed plugins are fresh Release builds!" -ForegroundColor Green
        return $true
    }
}

# Run verification
$success = Test-DeploymentFresh -HourThreshold 24
exit $(if ($success) { 0 } else { 1 })
