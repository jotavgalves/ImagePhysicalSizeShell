param(
    [switch]$DryRun,
    [string]$OutDir = ".\repair-dry-run"
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "ImagePhysicalSizeShell.Installation.psm1") -Force
$sourceRoot = Split-Path -Parent $PSScriptRoot
$audit = Get-FullAudit
$plan = New-InstallPlan -Audit $audit -SourceRoot $sourceRoot
$preconditions = Test-InstallPreconditions -Plan $plan -RequireBuiltArtifacts

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
[ordered]@{
    schemaVersion = 1
    mode = "repair-dry-run"
    generatedAt = (Get-Date).ToString("o")
    preconditions = $preconditions
    note = "Repair is planning-only in this phase. No files or registry values were modified."
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutDir "repair-plan.json") -Encoding UTF8

Write-Host "Repair dry-run written to $OutDir"
if (-not $preconditions.ok) {
    Write-Host "Safe repair would be blocked:"
    foreach ($failure in $preconditions.failures) { Write-Host " - $failure" }
}

