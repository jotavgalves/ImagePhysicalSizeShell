param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDir = "build",
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"

if (-not [Environment]::Is64BitOperatingSystem) {
    throw "Only Windows x64 is supported."
}

function Find-VsDevCmd {
    $candidates = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    )

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installPath) {
            $candidates = @((Join-Path $installPath "Common7\Tools\VsDevCmd.bat")) + $candidates
        }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }
    return $null
}

function Import-VsEnvironment {
    param([string]$VsDevCmd)
    if (-not $VsDevCmd) {
        return
    }

    $temp = [System.IO.Path]::GetTempFileName()
    try {
        cmd.exe /s /c "`"$VsDevCmd`" -arch=x64 -host_arch=x64 >nul && set > `"$temp`""
        Get-Content -LiteralPath $temp | ForEach-Object {
            $idx = $_.IndexOf("=")
            if ($idx -gt 0) {
                $name = $_.Substring(0, $idx)
                $value = $_.Substring($idx + 1)
                Set-Item -Path "Env:\$name" -Value $value
            }
        }
    } finally {
        Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
    }
}

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vsDevCmd = Find-VsDevCmd
    if ($vsDevCmd) {
        Import-VsEnvironment -VsDevCmd $vsDevCmd
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake was not found in PATH. Run from a Visual Studio 2022 Developer PowerShell with CMake installed."
}

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    throw "MSVC cl.exe was not found. Install Visual Studio 2022/Build Tools with the Desktop development with C++ workload."
}

cmake -S . -B $BuildDir -A x64
cmake --build $BuildDir --config $Configuration

if ($RunTests) {
    ctest --test-dir $BuildDir -C $Configuration --output-on-failure
}
