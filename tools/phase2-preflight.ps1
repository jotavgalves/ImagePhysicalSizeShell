param(
    [string]$OutDir = ".\phase2-preflight"
)

$ErrorActionPreference = "Stop"

$out = New-Item -ItemType Directory -Force -Path $OutDir

function Find-VsDevCmd {
    $candidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    )
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installPath) {
            $candidates = @((Join-Path $installPath "Common7\Tools\VsDevCmd.bat")) + $candidates
        }
    }
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) { return $candidate }
    }
    return $null
}

function Import-VsEnvironment {
    param([string]$VsDevCmd)
    if (-not $VsDevCmd) { return }
    $temp = [System.IO.Path]::GetTempFileName()
    try {
        cmd.exe /s /c "`"$VsDevCmd`" -arch=x64 -host_arch=x64 >nul && set > `"$temp`""
        Get-Content -LiteralPath $temp | ForEach-Object {
            $idx = $_.IndexOf("=")
            if ($idx -gt 0) {
                Set-Item -Path ("Env:\" + $_.Substring(0, $idx)) -Value $_.Substring($idx + 1)
            }
        }
    } finally {
        Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
    }
}

$vsDevCmd = Find-VsDevCmd
if ($vsDevCmd) {
    Import-VsEnvironment -VsDevCmd $vsDevCmd
}

function Find-CommandInfo {
    param([string]$Name)
    $cmds = Get-Command $Name -All -ErrorAction SilentlyContinue
    if (-not $cmds) {
        return @([ordered]@{ name = $Name; found = $false; source = $null; version = $null })
    }
    return @($cmds | ForEach-Object {
        [ordered]@{
            name = $Name
            found = $true
            source = $_.Source
            version = if ($_.Version) { $_.Version.ToString() } else { $null }
        }
    })
}

$paths = @(
    "C:\Program Files\Microsoft Visual Studio",
    "C:\Program Files (x86)\Microsoft Visual Studio",
    "C:\Program Files\CMake",
    "C:\Program Files (x86)\CMake",
    "C:\Program Files (x86)\Windows Kits\10",
    "C:\Program Files\LLVM",
    "C:\Program Files\Ninja"
)

$registryKeys = @(
    "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\Setup\Instances",
    "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\Setup\Instances",
    "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Kits\Installed Roots"
)

$report = [ordered]@{
    schemaVersion = 1
    phase = "2-preflight"
    generatedAt = (Get-Date).ToString("o")
    vsDevCmd = $vsDevCmd
    windowsVersion = [Environment]::OSVersion.VersionString
    is64BitOperatingSystem = [Environment]::Is64BitOperatingSystem
    is64BitProcess = [Environment]::Is64BitProcess
    commands = @(
        Find-CommandInfo cmake
        Find-CommandInfo cl
        Find-CommandInfo msbuild
        Find-CommandInfo ninja
        Find-CommandInfo clang-cl
        Find-CommandInfo ctest
    )
    paths = @($paths | ForEach-Object { [ordered]@{ path = $_; exists = Test-Path -LiteralPath $_ } })
    registryKeys = @($registryKeys | ForEach-Object { [ordered]@{ path = $_; exists = Test-Path -LiteralPath $_ } })
}

$jsonPath = Join-Path $out "phase2-preflight.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

Write-Host "Phase 2 preflight written to $jsonPath"
Write-Host ""
Write-Host "Toolchain summary:"
$report.commands | ForEach-Object {
    foreach ($item in $_) {
        Write-Host ("{0,-10} {1}" -f $item.name, $(if ($item.found) { $item.source } else { "(not found)" }))
    }
}
