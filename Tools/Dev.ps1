# Local development helper for AirPods for Windows.
#
#   pwsh Tools/Dev.ps1 configure   Configure the build with the "local" CMake preset
#   pwsh Tools/Dev.ps1 build       Build the app, tests and installer (configures first if needed)
#   pwsh Tools/Dev.ps1 test        Run the tests that do not open windows
#   pwsh Tools/Dev.ps1 test-ui     Run every test, including ones that open windows on screen
#   pwsh Tools/Dev.ps1 install     Build, back up settings, install over the current copy, relaunch
#   pwsh Tools/Dev.ps1 run         Start the installed app
#
# The "local" preset lives in CMakeUserPresets.json, which holds machine-specific paths and is not
# committed. See CLAUDE.md for its expected contents.

param(
    [ValidateSet('configure', 'build', 'test', 'test-ui', 'install', 'run')]
    [string]$Command = 'build'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$userPresets = Join-Path $repoRoot 'CMakeUserPresets.json'
$installedExe = Join-Path $env:ProgramFiles 'AirPodsDesktop\AirPodsDesktop.exe'

function Invoke-Checked([string]$FilePath, [string[]]$Arguments) {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath $($Arguments -join ' ') failed with exit code $LASTEXITCODE."
    }
}

function Get-BinaryDir {
    if (-not (Test-Path -LiteralPath $userPresets)) {
        throw "CMakeUserPresets.json is missing. Create it as described in CLAUDE.md."
    }
    $presets = Get-Content -LiteralPath $userPresets -Raw | ConvertFrom-Json
    return ($presets.configurePresets | Where-Object name -eq 'local').binaryDir
}

function Invoke-Configure {
    Invoke-Checked cmake @('--preset', 'local')
}

function Invoke-Build {
    if (-not (Test-Path -LiteralPath (Join-Path (Get-BinaryDir) 'CMakeCache.txt'))) {
        Invoke-Configure
    }
    # No --parallel: compiling the embedded resources in parallel with other targets has run the
    # compiler out of heap space.
    Invoke-Checked cmake @('--build', '--preset', 'local')
}

function Start-App {
    if (-not (Test-Path -LiteralPath $installedExe)) {
        throw "The app is not installed at $installedExe. Run: pwsh Tools/Dev.ps1 install"
    }
    Start-Process -FilePath $installedExe
}

function Invoke-Install {
    Invoke-Build

    $installer = Get-ChildItem -LiteralPath (Join-Path (Get-BinaryDir) 'Installer') -Filter '*-win64.exe' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $installer) {
        throw 'No installer was generated.'
    }

    $backupDir = Join-Path $env:APPDATA 'AirPodsDesktop\SettingsBackups'
    New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
    $backup = Join-Path $backupDir ("settings-{0:yyyyMMdd-HHmmss}.reg" -f (Get-Date))
    & reg export 'HKCU\Software\AirPodsDesktop' $backup /y | Out-Null
    Write-Host "Settings backed up to $backup"

    Get-Process AirPodsDesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Write-Host "Installing $($installer.Name) (approve the administrator prompt)..."
    $process = Start-Process -FilePath $installer.FullName -ArgumentList '/S' -Verb RunAs -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "The installer failed with exit code $($process.ExitCode)."
    }

    Start-App
    Write-Host 'Installed and started.'
}

Push-Location $repoRoot
try {
    switch ($Command) {
        'configure' { Invoke-Configure }
        'build' { Invoke-Build }
        'test' { Invoke-Checked ctest @('--preset', 'local') }
        'test-ui' {
            Write-Warning 'These tests open app windows on screen, some in Chinese, for a few seconds each.'
            Invoke-Checked ctest @('--preset', 'local-ui')
        }
        'install' { Invoke-Install }
        'run' { Start-App }
    }
}
finally {
    Pop-Location
}
