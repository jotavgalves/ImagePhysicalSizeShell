param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

.\build.ps1 -Configuration $Configuration -BuildDir $BuildDir -RunTests

