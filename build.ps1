[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$BuildDirectory = 'build',

    [switch]$RunTests,

    [switch]$NoPause
)

$ErrorActionPreference = 'Stop'
$exitCode = 0

try {
    $projectDirectory = $PSScriptRoot
    $vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'C:\vcpkg' }
    $toolchainFile = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

    if (-not (Test-Path -LiteralPath $toolchainFile -PathType Leaf)) {
        throw "vcpkg toolchain was not found: $toolchainFile. Install vcpkg or set VCPKG_ROOT."
    }

    Write-Host "Configuring WardogsKillTracker ($Configuration)..."
    # --fresh prevents an older cache (created without vcpkg) from hiding packages.
    & cmake --fresh -S $projectDirectory -B $BuildDirectory -A x64 `
        "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE."
    }

    Write-Host "Building WardogsKillTracker ($Configuration)..."
    & cmake --build $BuildDirectory --config $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    if ($RunTests) {
        Write-Host "Running tests ($Configuration)..."
        & ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            throw "Tests failed with exit code $LASTEXITCODE."
        }
    }

    $executable = Join-Path $BuildDirectory "$Configuration\WardogsKillTracker.exe"
    Write-Host "`nBuild completed successfully: $executable" -ForegroundColor Green
}
catch {
    $exitCode = 1
    Write-Host "`nERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "The full error output is shown above." -ForegroundColor Yellow
}
finally {
    [Environment]::ExitCode = $exitCode

    if (-not $NoPause) {
        Write-Host ''
        [void](Read-Host 'Press Enter to close this window')
    }
}
