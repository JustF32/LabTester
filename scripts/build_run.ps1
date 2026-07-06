[CmdletBinding()]
param(
    [string]$QtBase = "",
    [string]$QtVersion = "",
    [string]$QtArch = "mingw_64",
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",
    [switch]$Clean,
    [switch]$NoDeploy,
    [switch]$NoRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-Step {
    param([string]$Message)
    Write-Host "`n==> $Message" -ForegroundColor Cyan
}

function Resolve-QtPrefix {
    param(
        [string]$BasePath,
        [string]$VersionHint,
        [string]$Arch
    )

    if ($VersionHint) {
        $explicit = Join-Path $BasePath (Join-Path $VersionHint $Arch)
        $qtConfig = Join-Path $explicit "lib\cmake\Qt6\Qt6Config.cmake"
        if (!(Test-Path $qtConfig)) {
            throw "Qt6Config.cmake not found at '$qtConfig'."
        }
        return (Resolve-Path $explicit).Path
    }

    $versionDirs = Get-ChildItem -Path $BasePath -Directory -ErrorAction Stop |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' }

    $candidates = @()
    foreach ($dir in $versionDirs) {
        $candidate = Join-Path $dir.FullName $Arch
        $qtConfig = Join-Path $candidate "lib\cmake\Qt6\Qt6Config.cmake"
        if (Test-Path $qtConfig) {
            $candidates += [PSCustomObject]@{
                Version = [version]$dir.Name
                Prefix  = (Resolve-Path $candidate).Path
            }
        }
    }

    if ($candidates.Count -eq 0) {
        throw "No Qt kit with Qt6Config.cmake found under '$BasePath' for arch '$Arch'."
    }

    return ($candidates | Sort-Object Version -Descending | Select-Object -First 1).Prefix
}

function Resolve-QtBase {
    param([string]$PreferredPath)

    if ($PreferredPath -and (Test-Path $PreferredPath)) {
        return (Resolve-Path $PreferredPath).Path
    }

    $candidates = @(
        $env:LABTESTER_QT_BASE,
        $env:QT_BASE_DIR,
        "C:\Qt",
        "D:\Qt",
        "E:\Qt"
    ) | Where-Object { $_ -and $_.Trim().Length -gt 0 }

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Qt base folder not found. Pass -QtBase or set LABTESTER_QT_BASE."
}

function Find-CMake {
    param([string]$BasePath)

    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $fallbacks = @(
        (Join-Path $BasePath "Tools\CMake_64\bin\cmake.exe"),
        (Join-Path $BasePath "Tools\CMake\bin\cmake.exe")
    )

    foreach ($file in $fallbacks) {
        if (Test-Path $file) {
            return $file
        }
    }

    throw "cmake.exe not found in PATH or Qt Tools folder."
}

function Find-Ninja {
    param([string]$BasePath)

    $cmd = Get-Command ninja -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $fallback = Join-Path $BasePath "Tools\Ninja\ninja.exe"
    if (Test-Path $fallback) {
        return $fallback
    }

    throw "ninja.exe not found in PATH or '$fallback'."
}

function Find-VcVars64 {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0 -and $installPath) {
            $candidate = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $fallbacks = @(
        "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    )

    foreach ($file in $fallbacks) {
        if (Test-Path $file) {
            return $file
        }
    }

    throw "vcvars64.bat not found. Install Visual Studio C++ workload or open Developer PowerShell."
}

function Find-VcRedistX64 {
    $envCandidates = @(
        $env:LABTESTER_VCREDIST_X64,
        $env:VC_REDIST_X64
    ) | Where-Object { $_ -and $_.Trim().Length -gt 0 }

    foreach ($candidate in $envCandidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0 -and $installPath) {
            $redistCandidates = Get-ChildItem -Path (Join-Path $installPath "VC\Redist\MSVC\*\vc_redist.x64.exe") -File -ErrorAction SilentlyContinue |
                Sort-Object FullName -Descending
            if ($redistCandidates.Count -gt 0) {
                return $redistCandidates[0].FullName
            }
        }
    }

    $fallbackPatterns = @(
        "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\vc_redist.x64.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\vc_redist.x64.exe",
        "C:\Program Files\Microsoft Visual Studio\2019\*\VC\Redist\MSVC\*\vc_redist.x64.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\*\VC\Redist\MSVC\*\vc_redist.x64.exe"
    )

    foreach ($pattern in $fallbackPatterns) {
        $matches = Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue | Sort-Object FullName -Descending
        if ($matches.Count -gt 0) {
            return $matches[0].FullName
        }
    }

    return $null
}

function Ensure-VcRedistInDeploy {
    param([string]$DeployDir)

    $target = Join-Path $DeployDir "vc_redist.x64.exe"
    if (Test-Path $target) {
        return
    }

    $source = Find-VcRedistX64
    if (-not $source) {
        Write-Warning "vc_redist.x64.exe not found. Set LABTESTER_VCREDIST_X64 to include VC++ runtime in installer."
        return
    }

    Copy-Item -Path $source -Destination $target -Force
    Write-Host "copied: vc_redist.x64.exe"
}

function Ensure-QpsqlPlugin {
    param(
        [string]$QtPrefix,
        [string]$DeployDir,
        [string]$Config
    )

    $deploySqlDir = Join-Path $DeployDir "sqldrivers"
    New-Item -ItemType Directory -Path $deploySqlDir -Force | Out-Null

    $qtSqlDir = Join-Path $QtPrefix "plugins\sqldrivers"
    if (!(Test-Path $qtSqlDir)) {
        Write-Warning "Qt sqldrivers folder not found: $qtSqlDir"
        return
    }

    $pluginsToTry = @("qsqlpsql.dll")
    if ($Config -eq "Debug") {
        $pluginsToTry += "qsqlpsqld.dll"
    }

    foreach ($pluginName in $pluginsToTry) {
        $target = Join-Path $deploySqlDir $pluginName
        if (Test-Path $target) {
            continue
        }

        $source = Join-Path $qtSqlDir $pluginName
        if (Test-Path $source) {
            Copy-Item -Path $source -Destination $target -Force
            Write-Host "copied: sqldrivers\\$pluginName"
        }
    }

    if (!(Test-Path (Join-Path $deploySqlDir "qsqlpsql.dll"))) {
        Write-Warning "qsqlpsql.dll is missing after deploy. Verify Qt SQL PostgreSQL plugin is installed in your Qt kit."
    }
}

function Invoke-WithVcVars {
    param(
        [string]$VcVarsPath,
        [string]$CommandLine
    )

    if ([string]::IsNullOrWhiteSpace($VcVarsPath)) {
        cmd /c $CommandLine
    } else {
        $full = "`"$VcVarsPath`" && $CommandLine"
        cmd /c $full
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $CommandLine"
    }
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

function Find-PostgresBin {
    $envCandidates = @(
        $env:LABTESTER_PG_BIN,
        $env:PG_BIN,
        $env:PGBIN
    ) | Where-Object { $_ -and $_.Trim().Length -gt 0 }

    foreach ($candidate in $envCandidates) {
        $libpq = Join-Path $candidate "libpq.dll"
        if (Test-Path $libpq) {
            return (Resolve-Path $candidate).Path
        }
    }

    if ($env:POSTGRESQL_HOME) {
        $homeBin = Join-Path $env:POSTGRESQL_HOME "bin"
        if (Test-Path (Join-Path $homeBin "libpq.dll")) {
            return (Resolve-Path $homeBin).Path
        }
    }

    $roots = @(
        "C:\Program Files\PostgreSQL",
        "C:\Program Files (x86)\PostgreSQL"
    )
    foreach ($root in $roots) {
        if (!(Test-Path $root)) {
            continue
        }
        $versions = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
        foreach ($ver in $versions) {
            $bin = Join-Path $ver.FullName "bin"
            if (Test-Path (Join-Path $bin "libpq.dll")) {
                return (Resolve-Path $bin).Path
            }
        }
    }

    return $null
}

function Copy-PostgresRuntime {
    param(
        [string]$TargetDir
    )

    $pgBin = Find-PostgresBin
    if (-not $pgBin) {
        Write-Warning "PostgreSQL runtime not found (libpq.dll). Set LABTESTER_PG_BIN or POSTGRESQL_HOME\bin."
        return
    }

    Write-Step "Copying PostgreSQL runtime from '$pgBin'"

    $copyNames = @(
        "libpq.dll",
        "libssl-3-x64.dll",
        "libcrypto-3-x64.dll",
        "libintl-8.dll",
        "libiconv-2.dll",
        "zlib1.dll"
    )

    foreach ($name in $copyNames) {
        $src = Join-Path $pgBin $name
        if (Test-Path $src) {
            Copy-Item -Path $src -Destination (Join-Path $TargetDir $name) -Force
            Write-Host "copied: $name"
        }
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
$buildPath = Join-Path $projectRoot $BuildDir

$runningLabTester = Get-Process LabTester -ErrorAction SilentlyContinue
if ($runningLabTester) {
    Write-Step "Stopping running LabTester"
    $runningLabTester | Stop-Process -Force
}

if ($Clean -and (Test-Path $buildPath)) {
    Write-Step "Cleaning build directory"
    Remove-Item -Recurse -Force $buildPath
}

$resolvedQtBase = Resolve-QtBase -PreferredPath $QtBase
$qtPrefix = Resolve-QtPrefix -BasePath $resolvedQtBase -VersionHint $QtVersion -Arch $QtArch
$qtBin = Join-Path $qtPrefix "bin"
$windeployqt = Join-Path $qtBin "windeployqt.exe"
$cmake = Find-CMake -BasePath $resolvedQtBase
$ninja = Find-Ninja -BasePath $resolvedQtBase
$vcvars = $null
if ($QtArch -match "msvc") {
    $vcvars = Find-VcVars64
}

Write-Step "Using tools"
Write-Host "Qt base   : $resolvedQtBase"
Write-Host "Qt prefix : $qtPrefix"
Write-Host "CMake     : $cmake"
Write-Host "Ninja     : $ninja"
Write-Host "vcvars64  : $vcvars"

Write-Step "Configuring CMake"
$configureCmd = "`"$cmake`" -S `"$projectRoot`" -B `"$buildPath`" -G Ninja -DCMAKE_PREFIX_PATH=`"$qtPrefix`" -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=$Configuration"
Invoke-WithVcVars -VcVarsPath $vcvars -CommandLine $configureCmd

Write-Step "Building project"
$buildCmd = "`"$cmake`" --build `"$buildPath`" --config $Configuration"
Invoke-WithVcVars -VcVarsPath $vcvars -CommandLine $buildCmd

$exePath = Resolve-ExePath -BuildPath $buildPath -Config $Configuration
Write-Step "Built executable"
Write-Host $exePath

if (-not $NoDeploy) {
    if (!(Test-Path $windeployqt)) {
        throw "windeployqt.exe not found: $windeployqt"
    }

    Write-Step "Deploying Qt runtime (windeployqt)"
    $deployCmd = "`"$windeployqt`" --qmldir `"$([IO.Path]::Combine($projectRoot, 'qml'))`" `"$exePath`""
    Invoke-WithVcVars -VcVarsPath $vcvars -CommandLine $deployCmd

    Ensure-QpsqlPlugin -QtPrefix $qtPrefix -DeployDir (Split-Path -Parent $exePath) -Config $Configuration
    Copy-PostgresRuntime -TargetDir (Split-Path -Parent $exePath)
    Ensure-VcRedistInDeploy -DeployDir (Split-Path -Parent $exePath)
}

if (-not $NoRun) {
    Write-Step "Starting LabTester"
    Start-Process -FilePath $exePath -WorkingDirectory (Split-Path -Parent $exePath)
}

Write-Step "Done"
