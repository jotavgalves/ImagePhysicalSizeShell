param(
    [string]$InstallDir = "C:\Program Files\ImagePhysicalSizeShell",
    [switch]$AllUsersStartMenu
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $InstallDir "ipsconfigui.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Configurador não encontrado: $exe"
}

$shell = New-Object -ComObject WScript.Shell

function New-Shortcut {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Target,
        [Parameter(Mandatory)][string]$WorkingDirectory,
        [string]$Description = "Configurar campos ImagePhysicalSizeShell"
    )

    $parent = Split-Path -Parent $Path
    if ($parent) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $shortcut = $shell.CreateShortcut($Path)
    $shortcut.TargetPath = $Target
    $shortcut.WorkingDirectory = $WorkingDirectory
    $shortcut.IconLocation = "$Target,0"
    $shortcut.Description = $Description
    $shortcut.Save()
}

$created = @()

$appShortcut = Join-Path $InstallDir "Abrir ImagePhysicalSize Config.lnk"
New-Shortcut -Path $appShortcut -Target $exe -WorkingDirectory $InstallDir
$created += $appShortcut

if ($AllUsersStartMenu) {
    $startMenu = Join-Path $env:ProgramData "Microsoft\Windows\Start Menu\Programs\ImagePhysicalSizeShell"
} else {
    $startMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\ImagePhysicalSizeShell"
}

$startShortcut = Join-Path $startMenu "ImagePhysicalSize Config.lnk"
New-Shortcut -Path $startShortcut -Target $exe -WorkingDirectory $InstallDir
$created += $startShortcut

$created | ForEach-Object { Write-Host "Atalho criado: $_" }
