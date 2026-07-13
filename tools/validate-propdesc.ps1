param(
    [string]$Path = ".\schema\ImagePhysicalSize.propdesc",
    [string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

$resolved = Resolve-Path -LiteralPath $Path
[xml]$xml = Get-Content -LiteralPath $resolved -Raw

$ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
$ns.AddNamespace("p", "http://schemas.microsoft.com/windows/2006/propertydescription")

$errors = New-Object System.Collections.Generic.List[string]
function Add-Error([string]$Message) { $errors.Add($Message) | Out-Null }

$schema = $xml.SelectSingleNode("/p:schema", $ns)
if (-not $schema) {
    Add-Error "Root element must be <schema> in the Windows propertydescription namespace."
} elseif ($schema.schemaVersion -ne "1.0") {
    Add-Error "schemaVersion must be 1.0."
}

$list = $xml.SelectSingleNode("/p:schema/p:propertyDescriptionList", $ns)
if (-not $list) { Add-Error "Missing propertyDescriptionList." }

$expectedFormatId = "{7A4E8B66-6C8A-421D-9868-42F50F9312B4}"
$expected = [ordered]@{
    "ImagePhysicalSizeShell.PhysicalWidthCm" = @{ propID = "2"; type = "Double"; displayType = "Number" }
    "ImagePhysicalSizeShell.PhysicalHeightCm" = @{ propID = "3"; type = "Double"; displayType = "Number" }
    "ImagePhysicalSizeShell.PhysicalSizeCm" = @{ propID = "4"; type = "String"; displayType = "String" }
    "ImagePhysicalSizeShell.EmbeddedDpiX" = @{ propID = "5"; type = "Double"; displayType = "Number" }
    "ImagePhysicalSizeShell.EmbeddedDpiY" = @{ propID = "6"; type = "Double"; displayType = "Number" }
    "ImagePhysicalSizeShell.DpiSource" = @{ propID = "7"; type = "String"; displayType = "String" }
    "ImagePhysicalSizeShell.DpiStatus" = @{ propID = "8"; type = "String"; displayType = "String" }
}

$nodes = @($xml.SelectNodes("//p:propertyDescription", $ns))
if ($nodes.Count -ne $expected.Count) {
    Add-Error "Expected $($expected.Count) propertyDescription elements, found $($nodes.Count)."
}

$seenNames = @{}
$seenPropIds = @{}
$properties = @()

foreach ($node in $nodes) {
    $name = [string]$node.name
    $propId = [string]$node.propID
    $formatId = [string]$node.formatID
    $typeInfo = $node.SelectSingleNode("p:typeInfo", $ns)
    $displayInfo = $node.SelectSingleNode("p:displayInfo", $ns)
    $searchInfo = $node.SelectSingleNode("p:searchInfo", $ns)
    $labelInfo = $node.SelectSingleNode("p:labelInfo", $ns)

    if ($seenNames.ContainsKey($name)) { Add-Error "Duplicate property name: $name" }
    $seenNames[$name] = $true
    if ($seenPropIds.ContainsKey($propId)) { Add-Error "Duplicate propID: $propId" }
    $seenPropIds[$propId] = $true

    if (-not $expected.Contains($name)) {
        Add-Error "Unexpected property name: $name"
    } else {
        $spec = $expected[$name]
        if ($propId -ne $spec.propID) { Add-Error "$name propID expected $($spec.propID), found $propId." }
        if ($formatId -ne $expectedFormatId) { Add-Error "$name formatID expected $expectedFormatId, found $formatId." }
        if (-not $typeInfo) {
            Add-Error "$name missing typeInfo."
        } elseif ([string]$typeInfo.type -ne $spec.type) {
            Add-Error "$name type expected $($spec.type), found $($typeInfo.type)."
        }
        if (-not $displayInfo) {
            Add-Error "$name missing displayInfo."
        } elseif ([string]$displayInfo.displayType -ne $spec.displayType) {
            Add-Error "$name displayType expected $($spec.displayType), found $($displayInfo.displayType)."
        }
    }

    if (-not $searchInfo) {
        Add-Error "$name missing searchInfo."
    } elseif ([string]$searchInfo.isColumn -ne "true") {
        Add-Error "$name searchInfo isColumn must be true."
    }
    if (-not $labelInfo -or [string]$labelInfo.label -eq "") {
        Add-Error "$name missing labelInfo label."
    }

    $properties += [ordered]@{
        name = $name
        propID = $propId
        formatID = $formatId
        type = if ($typeInfo) { [string]$typeInfo.type } else { $null }
        displayType = if ($displayInfo) { [string]$displayInfo.displayType } else { $null }
        label = if ($labelInfo) { [string]$labelInfo.label } else { $null }
        isColumn = if ($searchInfo) { [string]$searchInfo.isColumn } else { $null }
    }
}

foreach ($name in $expected.Keys) {
    if (-not $seenNames.ContainsKey($name)) { Add-Error "Missing property: $name" }
}

$report = [ordered]@{
    schemaVersion = 1
    path = $resolved.Path
    validatedAt = (Get-Date).ToString("o")
    ok = ($errors.Count -eq 0)
    propertyCount = $nodes.Count
    errors = @($errors)
    properties = $properties
}

if ($OutFile) {
    $parent = Split-Path -Parent $OutFile
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutFile -Encoding UTF8
}

if ($errors.Count -eq 0) {
    Write-Host "propdesc validation passed: $($nodes.Count) properties"
    exit 0
}

Write-Error ("propdesc validation failed: " + ($errors -join "; "))

