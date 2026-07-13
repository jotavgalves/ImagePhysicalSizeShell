param(
    [switch]$Install,
    [switch]$DryRun,
    [switch]$Verify,
    [switch]$NonInteractive,
    [string]$Authorization = "",
    [string]$OutDir = ".\install-dry-run"
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "ImagePhysicalSizeShell.Installation.psm1") -Force
$sourceRoot = Split-Path -Parent $PSScriptRoot

if ($Verify) {
    $audit = Get-FullAudit
    $plan = New-InstallPlan -Audit $audit -SourceRoot $sourceRoot
    $preconditions = Test-InstallPreconditions -Plan $plan -RequireBuiltArtifacts
    [ordered]@{
        ok = $preconditions.ok
        failures = $preconditions.failures
        aliasChecks = $plan.aliasChecks
        buildArtifacts = $plan.buildArtifacts
    } | ConvertTo-Json -Depth 10
    if (-not $preconditions.ok) { exit 1 }
    exit 0
}

if ($DryRun -or -not $Install) {
    $manifest = Invoke-InstallDryRun -SourceRoot $sourceRoot -OutDir $OutDir
    Write-Host "Dry-run manifest written to $OutDir"
    Write-Host "Operations planned: $($manifest.plan.operations.Count)"
    if (-not $manifest.preconditions.ok) {
        Write-Host "Real install would be blocked:"
        foreach ($failure in $manifest.preconditions.failures) {
            Write-Host " - $failure"
        }
    }
    exit 0
}

$audit = Get-FullAudit
$plan = New-InstallPlan -Audit $audit -SourceRoot $sourceRoot
$preconditions = Test-InstallPreconditions -Plan $plan -RequireBuiltArtifacts -RequireElevation
Assert-RealInstallAllowed -Authorization $Authorization -Preconditions $preconditions

$result = Invoke-RealInstall -SourceRoot $sourceRoot
$result | ConvertTo-Json -Depth 8
