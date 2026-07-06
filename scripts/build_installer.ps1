[CmdletBinding()]
param(
    [string]$QtBase = "",
    [string]$QtVersion = "",
    [string]$QtArch = "msvc2022_64",
    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [string]$AppVersion = "0.2v",
    [string]$StageDir = "",
    [string]$OutputDir = "dist",
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-Step {
    param([string]$Message)
    Write-Host "`n==> $Message" -ForegroundColor Cyan
}

function Resolve-Iscc {
    $cmd = Get-Command iscc.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $fallbacks = @(
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $fallbacks) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "ISCC.exe not found. Install Inno Setup 6 or add ISCC.exe to PATH."
}

function Resolve-ExePath {
    param(
        [string]$BuildPath,
        [string]$Config
    )

    $candidates = @(
        (Join-Path $BuildPath "LabTester.exe"),
        (Join-Path $BuildPath (Join-Path $Config "LabTester.exe"))
    )
    foreach ($file in $candidates) {
        if (Test-Path $file) {
            return (Resolve-Path $file).Path
        }
    }

    throw "LabTester.exe not found in '$BuildPath'."
}

function Copy-IfExists {
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )
    if (Test-Path $SourcePath) {
        Copy-Item -Path $SourcePath -Destination $DestinationPath -Force
    }
}

function Copy-DirIfExists {
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )
    if (Test-Path $SourcePath) {
        Copy-Item -Path $SourcePath -Destination $DestinationPath -Recurse -Force
    }
}

function Assert-RequiredStageFiles {
    param([string]$StagePath)

    $requiredRelativePaths = @(
        "LabTester.exe",
        "LabTesterWorker.exe",
        "sqldrivers\\qsqlpsql.dll",
        "libpq.dll",
        "tls\\qschannelbackend.dll",
        "vc_redist.x64.exe"
    )

    $missing = @()
    foreach ($rel in $requiredRelativePaths) {
        $fullPath = Join-Path $StagePath $rel
        if (!(Test-Path $fullPath)) {
            $missing += $rel
        }
    }

    if ($missing.Count -gt 0) {
        $details = ($missing | ForEach-Object { " - $_" }) -join [Environment]::NewLine
        throw "Installer stage is missing required files:`n$details`nRun scripts/build_run.ps1 and ensure Qt SQL (QPSQL), PostgreSQL runtime and VC++ redist are available."
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
$buildPath = Join-Path $projectRoot $BuildDir
$issPath = Join-Path $projectRoot "installer\LabTester.iss"

if (!(Test-Path $issPath)) {
    throw "Installer script not found: $issPath"
}

if (!$NoBuild) {
    Write-Step "Building and deploying runtime"
    $buildScript = Join-Path $scriptDir "build_run.ps1"
    & $buildScript `
        -QtBase $QtBase `
        -QtVersion $QtVersion `
        -QtArch $QtArch `
        -BuildDir $BuildDir `
        -Configuration $Configuration `
        -Clean `
        -NoRun

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}

$exePath = Resolve-ExePath -BuildPath $buildPath -Config $Configuration
$deployDir = Split-Path -Parent $exePath

if ([string]::IsNullOrWhiteSpace($StageDir)) {
    $StageDir = Join-Path $buildPath "installer_stage"
} elseif (-not [System.IO.Path]::IsPathRooted($StageDir)) {
    $StageDir = Join-Path $projectRoot $StageDir
}

if (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $projectRoot $OutputDir
}

if (Test-Path $StageDir) {
    Remove-Item -Path $StageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

Write-Step "Staging application runtime"

Copy-IfExists -SourcePath (Join-Path $deployDir "LabTester.exe") -DestinationPath (Join-Path $StageDir "LabTester.exe")
Copy-IfExists -SourcePath (Join-Path $deployDir "LabTesterWorker.exe") -DestinationPath (Join-Path $StageDir "LabTesterWorker.exe")
Copy-IfExists -SourcePath (Join-Path $deployDir "qt.conf") -DestinationPath (Join-Path $StageDir "qt.conf")
Copy-IfExists -SourcePath (Join-Path $deployDir "vc_redist.x64.exe") -DestinationPath (Join-Path $StageDir "vc_redist.x64.exe")

$filePatterns = @("*.dll", "*.qm", "*.pak", "*.dat")
foreach ($pattern in $filePatterns) {
    $items = Get-ChildItem -Path (Join-Path $deployDir $pattern) -File -ErrorAction SilentlyContinue
    foreach ($item in $items) {
        Copy-Item -Path $item.FullName -Destination (Join-Path $StageDir $item.Name) -Force
    }
}

$pluginDirs = @(
    "platforms",
    "styles",
    "iconengines",
    "imageformats",
    "networkinformation",
    "tls",
    "sqldrivers",
    "qml",
    "translations",
    "generic",
    "platformthemes"
)
foreach ($dirName in $pluginDirs) {
    Copy-DirIfExists -SourcePath (Join-Path $deployDir $dirName) -DestinationPath (Join-Path $StageDir $dirName)
}

$labsPath = Join-Path $projectRoot "labs"
Copy-DirIfExists -SourcePath $labsPath -DestinationPath (Join-Path $StageDir "labs")

Assert-RequiredStageFiles -StagePath $StageDir

$readmePath = Join-Path $StageDir "README_installer.txt"
@"
LabTester installer package

Included:
- LabTester application runtime
- LabTesterWorker server runtime
- Qt runtime dependencies (deployed by windeployqt)
- PostgreSQL runtime for QPSQL (libpq/OpenSSL)
- Microsoft Visual C++ Redistributable installer (auto-run)
- labs directory (initial templates/tests)

Not included:
- Local testing toolchain (MSVC/CMake/Ninja/GTest build environment)

For local test execution, install required C++ build tools separately.
"@ | Set-Content -Path $readmePath -Encoding UTF8

$iscc = Resolve-Iscc

Write-Step "Building installer with Inno Setup"
& $iscc `
    "/DMyAppVersion=$AppVersion" `
    "/DMyStageDir=$StageDir" `
    "/DMyOutputDir=$OutputDir" `
    $issPath

if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE."
}

Write-Step "Done"
Write-Host "Stage   : $StageDir"
Write-Host "Output  : $OutputDir"
