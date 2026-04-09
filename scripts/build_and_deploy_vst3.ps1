[CmdletBinding()]
param(
    [string]$WorkspaceFolder = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$Config = "Release",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

# Warn if Debug is explicitly requested (likely a mistake)
if ($Config -eq "Debug" -and -not $Force) {
    Write-Warning "You are building Debug configuration for deployment."
    Write-Warning "Debug builds are large (~19MB) with symbols and are NOT recommended for DAW use."
    Write-Warning "Use Release builds instead: .\build_and_deploy_vst3.ps1 -Config Release"
    Write-Host "`nContinue with Debug? (y/n): " -NoNewline -ForegroundColor Yellow
    $response = Read-Host
    if ($response -ne 'y' -and $response -ne 'Y') {
        Write-Host "Aborted. Use -Config Release or -Force to override." -ForegroundColor Cyan
        exit 0
    }
}

$buildDir = Join-Path $WorkspaceFolder "build"
$targetFromEnv = -not [string]::IsNullOrWhiteSpace($Env:VST3_TARGET)
$targetDir = $Env:VST3_TARGET
if (-not $targetFromEnv) {
    $targetDir = "C:\Program Files\Common Files\VST3"
}

Write-Host "Building $Config configuration..."
& cmake --build $buildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $targetDir)) {
    try {
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    }
    catch {
        if ($targetFromEnv) {
            throw
        }
        $fallbackTarget = Join-Path $WorkspaceFolder "deployed_vst3"
        Write-Warning "Cannot create default target '$targetDir'. Falling back to '$fallbackTarget'."
        if (-not (Test-Path -LiteralPath $fallbackTarget)) {
            New-Item -ItemType Directory -Path $fallbackTarget -Force | Out-Null
        }
        $targetDir = $fallbackTarget
    }
}

$allBundles = Get-ChildItem -Path $buildDir -Filter *.vst3 -Recurse -Directory |
    Where-Object { $_.FullName -notlike "*\CMakeFiles\*" }

if (-not $allBundles) {
    Write-Warning "No VST3 bundles found under $buildDir."
    exit 0
}

$configPattern = "*\$Config\VST3\*"
$bundles = $allBundles | Where-Object { $_.FullName -like $configPattern }

if (-not $bundles) {
    Write-Warning "No VST3 bundles found for config '$Config'. Falling back to newest bundle per plugin name."
    $bundles = $allBundles
}

# Keep one source bundle per plugin name to avoid duplicate deploys.
$bundles = $bundles |
    Sort-Object @{ Expression = 'Name'; Ascending = $true },
                @{ Expression = 'LastWriteTime'; Ascending = $false } |
    Group-Object Name |
    ForEach-Object { $_.Group | Select-Object -First 1 } |
    Sort-Object Name

foreach ($bundle in $bundles) {
    $destination = Join-Path $targetDir $bundle.Name
    if (Test-Path -LiteralPath $destination) {
        try {
            Remove-Item -LiteralPath $destination -Recurse -Force -ErrorAction Stop
        }
        catch {
            throw "Failed to remove existing deployed bundle '$destination'. Close any DAW/host that may be locking the plugin and try again. $($_.Exception.Message)"
        }
    }

    try {
        Copy-Item -LiteralPath $bundle.FullName -Destination $targetDir -Recurse -Force -ErrorAction Stop
    }
    catch {
        throw "Failed to copy '$($bundle.FullName)' to '$targetDir'. $($_.Exception.Message)"
    }

    Write-Host "Deployed $($bundle.Name) from $($bundle.FullName)"
}

Write-Host "Deploy complete: $targetDir"
