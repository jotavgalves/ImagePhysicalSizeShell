param(
    [string]$BackupRoot = "C:\ProgramData\ImagePhysicalSizeShell\backup",
    [string]$ManifestPath = "",
    [switch]$DryRun,
    [string]$OutDir = ".\rollback-dry-run"
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "ImagePhysicalSizeShell.Installation.psm1") -Force

if (-not $ManifestPath) {
    $ManifestPath = Join-Path $BackupRoot "install-manifest.json"
}

if ($DryRun) {
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

    if (Test-Path -LiteralPath $ManifestPath) {
        $manifest = Read-ManifestFile -Path $ManifestPath
        $plan = New-RollbackPlanFromManifest -Manifest $manifest
        $plan.manifestPath = $ManifestPath
        Write-JsonFile -Object $plan -Path (Join-Path $OutDir "rollback-plan.json")
    } else {
        [ordered]@{
            schemaVersion = 1
            mode = "rollback-dry-run"
            generatedAt = (Get-Date).ToString("o")
            backupRoot = $BackupRoot
            manifestPath = $ManifestPath
            manifestExists = $false
            note = "No manifest exists. No registry values or files were modified."
        } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutDir "rollback-plan.json") -Encoding UTF8
    }

    Write-Host "Rollback dry-run written to $OutDir"
    exit 0
}

if (-not (Test-Path -LiteralPath $ManifestPath)) {
    throw "Manifest not found: $ManifestPath"
}

$manifest = Read-ManifestFile -Path $ManifestPath
$messages = Invoke-RollbackFromManifest -Manifest $manifest -LogPath (Join-Path (Split-Path -Parent $ManifestPath) "manual-rollback.log")
$messages | ForEach-Object { Write-Host $_ }
