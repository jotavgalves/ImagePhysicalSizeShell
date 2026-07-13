param(
    [string]$OutDir = ".\audit"
)

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot "ImagePhysicalSizeShell.Installation.psm1") -Force

$out = Resolve-Path -LiteralPath (New-Item -ItemType Directory -Force -Path $OutDir)
$audit = Get-FullAudit
$path = Join-Path $out "readonly-powershell-registry-audit.json"
Write-JsonFile -Object $audit -Path $path

Write-Host "Read-only audit written to $path"
foreach ($item in $audit.extensions) {
    $clsid = $item.hklmPropertyHandler.data
    if (-not $clsid) { $clsid = "(missing)" }
    Write-Host ("{0,-6} {1}" -f $item.extension, $clsid)
}

