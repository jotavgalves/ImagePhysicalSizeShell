param(
    [switch]$Uninstall,
    [switch]$DryRun,
    [switch]$Force,
    [switch]$KeepBackup,
    [switch]$NoRestartExplorer,
    [string]$ManifestPath = "C:\ProgramData\ImagePhysicalSizeShell\backup\install-manifest.json",
    [string]$OutDir = ".\uninstall-dry-run"
)

$ErrorActionPreference = "Stop"

if ($DryRun -or -not $Uninstall) {
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    Import-Module (Join-Path $PSScriptRoot "ImagePhysicalSizeShell.Installation.psm1") -Force

    if (Test-Path -LiteralPath $ManifestPath) {
        $manifest = Read-ManifestFile -Path $ManifestPath
        $report = New-RollbackPlanFromManifest -Manifest $manifest
        $report.mode = "uninstall-dry-run"
        $report.manifestPath = $ManifestPath
        Write-JsonFile -Object $report -Path (Join-Path $OutDir "uninstall-plan.json")
    } else {
        $report = [ordered]@{
            schemaVersion = 1
            mode = "uninstall-dry-run"
            generatedAt = (Get-Date).ToString("o")
            manifestPath = $ManifestPath
            manifestExists = $false
            note = "No manifest found. No registry values or files were modified."
        }
        $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutDir "uninstall-plan.json") -Encoding UTF8
    }
    Write-Host "Uninstall dry-run written to $OutDir"
    exit 0
}

if (-not $Force) {
    throw "Real uninstall requires -Force after reviewing the rollback plan. Use -DryRun first."
}

if (-not (Test-Path -LiteralPath $ManifestPath)) {
    Write-Host "No manifest found. Nothing to uninstall."
    exit 0
}

Import-Module (Join-Path $PSScriptRoot "ImagePhysicalSizeShell.Installation.psm1") -Force
$manifest = Read-ManifestFile -Path $ManifestPath
$messages = Invoke-RollbackFromManifest -Manifest $manifest -LogPath (Join-Path (Split-Path -Parent $ManifestPath) "uninstall.log")
$messages | ForEach-Object { Write-Host $_ }

if (-not $KeepBackup) {
    Write-Host "Backup kept by default for safety. Remove C:\ProgramData\ImagePhysicalSizeShell\backup manually after verification."
}
