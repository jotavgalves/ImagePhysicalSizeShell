Set-StrictMode -Version Latest

$script:SupportedExtensions = ".png", ".jpg", ".jpeg", ".tif", ".tiff"
$script:PropertyListNames = "FullDetails", "PreviewDetails", "PreviewTitle", "InfoTip", "TileInfo", "ExtendedTileInfo"
$script:ProjectClsid = "{D75F9AC7-4664-46F3-A1D4-881140DF6CBE}"
$script:ProjectInstallDir = "C:\Program Files\ImagePhysicalSizeShell"
$script:BackupRoot = "C:\ProgramData\ImagePhysicalSizeShell\backup"
$script:PropertyHandlerRoot = "SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers"
$script:PrivateOriginalRoot = "SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers"
$script:ShellPropertyHandlerClsid = "{e357fccd-a995-4576-b01f-234630154e96}"
$script:ProjectProperties = @(
    "ImagePhysicalSizeShell.PhysicalSizeCm",
    "ImagePhysicalSizeShell.PhysicalWidthCm",
    "ImagePhysicalSizeShell.PhysicalHeightCm",
    "ImagePhysicalSizeShell.EmbeddedDpiX",
    "ImagePhysicalSizeShell.EmbeddedDpiY",
    "ImagePhysicalSizeShell.DpiSource",
    "ImagePhysicalSizeShell.DpiStatus"
)

function Get-Sha256FileHash {
    param([Parameter(Mandatory)][string]$Path)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $bytes = $sha.ComputeHash($stream)
        return (($bytes | ForEach-Object { $_.ToString("x2") }) -join "").ToUpperInvariant()
    } finally {
        $stream.Dispose()
        $sha.Dispose()
    }
}

function Get-RegistryProviderRoot {
    param([Parameter(Mandatory)][ValidateSet("HKLM", "HKCU", "HKCR")][string]$Hive)
    switch ($Hive) {
        "HKLM" { "Registry::HKEY_LOCAL_MACHINE" }
        "HKCU" { "Registry::HKEY_CURRENT_USER" }
        "HKCR" { "Registry::HKEY_CLASSES_ROOT" }
    }
}

function ConvertTo-RegistryProviderPath {
    param(
        [Parameter(Mandatory)][ValidateSet("HKLM", "HKCU", "HKCR")][string]$Hive,
        [Parameter(Mandatory)][string]$Path
    )
    "$(Get-RegistryProviderRoot -Hive $Hive)\$Path"
}

function Read-RegistryValueSnapshot {
    param(
        [Parameter(Mandatory)][ValidateSet("HKLM", "HKCU", "HKCR")][string]$Hive,
        [Parameter(Mandatory)][string]$Path,
        [string]$Name = "",
        [string]$Extension = ""
    )

    $providerPath = ConvertTo-RegistryProviderPath -Hive $Hive -Path $Path
    $snapshot = [ordered]@{
        hive = $Hive
        path = $Path
        name = $(if ($Name) { $Name } else { "(Default)" })
        rawName = $Name
        existed = $false
        type = $null
        data = $null
        extension = $Extension
    }

    if (-not (Test-Path -LiteralPath $providerPath)) {
        return $snapshot
    }

    $key = Get-Item -LiteralPath $providerPath
    if ($key.GetValueNames() -notcontains $Name) {
        return $snapshot
    }

    $snapshot.existed = $true
    $snapshot.type = $key.GetValueKind($Name).ToString()
    $snapshot.data = $key.GetValue($Name, $null, "DoNotExpandEnvironmentNames")
    return $snapshot
}

function Get-PropertyListSnapshots {
    param(
        [Parameter(Mandatory)][string]$BasePath,
        [string]$Extension = ""
    )
    foreach ($name in $script:PropertyListNames) {
        Read-RegistryValueSnapshot -Hive HKCR -Path $BasePath -Name $name -Extension $Extension
    }
}

function Get-ExtensionAudit {
    param([Parameter(Mandatory)][string]$Extension)

    $extKey = Read-RegistryValueSnapshot -Hive HKCR -Path $Extension -Extension $Extension
    $progId = if ($extKey.existed) { [string]$extKey.data } else { "" }

    [ordered]@{
        extension = $Extension
        hklmPropertyHandler = Read-RegistryValueSnapshot -Hive HKLM -Path "$script:PropertyHandlerRoot\$Extension" -Extension $Extension
        hkcuPropertyHandler = Read-RegistryValueSnapshot -Hive HKCU -Path "$script:PropertyHandlerRoot\$Extension" -Extension $Extension
        privateOriginalHandler = Read-RegistryValueSnapshot -Hive HKLM -Path "$script:PrivateOriginalRoot\$Extension" -Name "OriginalClsid" -Extension $Extension
        hkcrDefaultProgId = $extKey
        hkcrContentType = Read-RegistryValueSnapshot -Hive HKCR -Path $Extension -Name "Content Type" -Extension $Extension
        hkcrPerceivedType = Read-RegistryValueSnapshot -Hive HKCR -Path $Extension -Name "PerceivedType" -Extension $Extension
        hkcrShellExPropertyHandler = Read-RegistryValueSnapshot -Hive HKCR -Path "$Extension\ShellEx\$script:ShellPropertyHandlerClsid" -Extension $Extension
        extensionPropertyLists = @(Get-PropertyListSnapshots -BasePath "SystemFileAssociations\$Extension" -Extension $Extension)
        progIdPropertyLists = @(if ($progId) { Get-PropertyListSnapshots -BasePath $progId -Extension $Extension } else { @() })
        imagePropertyLists = @(Get-PropertyListSnapshots -BasePath "SystemFileAssociations\image" -Extension $Extension)
    }
}

function Get-FullAudit {
    $items = @()
    foreach ($ext in $script:SupportedExtensions) {
        $items += Get-ExtensionAudit -Extension $ext
    }
    [ordered]@{
        schemaVersion = 1
        generatedAt = (Get-Date).ToString("o")
        computerName = $env:COMPUTERNAME
        user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
        windowsVersion = [Environment]::OSVersion.VersionString
        is64BitOperatingSystem = [Environment]::Is64BitOperatingSystem
        is64BitProcess = [Environment]::Is64BitProcess
        extensions = $items
    }
}

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Test-ProjectBuildArtifacts {
    param([string]$SourceRoot)
    $candidates = @(
        (Join-Path $SourceRoot "build\Release\ImagePhysicalSizeShell.dll"),
        (Join-Path $SourceRoot "build\Release\ipsdiag.exe"),
        (Join-Path $SourceRoot "build\Release\ipsconfigui.exe"),
        (Join-Path $SourceRoot "build\Release\RegisterSchema.exe")
    )
    foreach ($path in $candidates) {
        [ordered]@{
            path = $path
            exists = Test-Path -LiteralPath $path
            sha256 = if (Test-Path -LiteralPath $path) { Get-Sha256FileHash -Path $path } else { $null }
        }
    }
}

function Test-AliasGroups {
    param([Parameter(Mandatory)]$Audit)

    $byExt = @{}
    foreach ($item in $Audit.extensions) {
        $byExt[$item.extension] = $item
    }

    $checks = @()
    foreach ($pair in @(@(".jpg", ".jpeg"), @(".tif", ".tiff"))) {
        $a = $byExt[$pair[0]].hklmPropertyHandler.data
        $b = $byExt[$pair[1]].hklmPropertyHandler.data
        $checks += [ordered]@{
            extensions = $pair
            handlerA = $a
            handlerB = $b
            ok = ($a -and $b -and ([string]$a).ToLowerInvariant() -eq ([string]$b).ToLowerInvariant())
        }
    }
    $checks
}

function Add-ProjectPropertiesToList {
    param([string]$Existing)

    $prefix = "prop:"
    $value = if ($Existing) { $Existing } else { $prefix }
    $body = if ($value.StartsWith($prefix)) { $value.Substring($prefix.Length) } else { $value }
    $parts = @($body -split ";" | Where-Object { $_ -ne "" })

    foreach ($property in $script:ProjectProperties) {
        $already = $false
        foreach ($part in $parts) {
            if ($part.TrimStart("*") -eq $property) {
                $already = $true
                break
            }
        }
        if (-not $already) {
            $parts += $property
        }
    }

    $prefix + ($parts -join ";")
}

function New-RegistryOperation {
    param(
        [Parameter(Mandatory)][ValidateSet("SetValue", "DeleteValue")][string]$Action,
        [Parameter(Mandatory)][ValidateSet("HKLM", "HKCU", "HKCR")][string]$Hive,
        [Parameter(Mandatory)][string]$Path,
        [string]$Name = "",
        [string]$Type = "String",
        $Data = $null,
        $Before = $null,
        [string]$Extension = "",
        [string]$Reason = ""
    )
    [ordered]@{
        kind = "registry"
        action = $Action
        hive = $Hive
        path = $Path
        name = $(if ($Name) { $Name } else { "(Default)" })
        rawName = $Name
        type = $Type
        data = $Data
        before = $Before
        extension = $Extension
        reason = $Reason
    }
}

function New-PlannedRegistrySetValue {
    param(
        [Parameter(Mandatory)][ValidateSet("HKLM", "HKCU", "HKCR")][string]$Hive,
        [Parameter(Mandatory)][string]$Path,
        [string]$Name = "",
        [string]$Type = "String",
        $Data = $null,
        [string]$Extension = "",
        [string]$Reason = ""
    )
    $before = Read-RegistryValueSnapshot -Hive $Hive -Path $Path -Name $Name -Extension $Extension
    New-RegistryOperation -Action SetValue -Hive $Hive -Path $Path -Name $Name -Type $Type -Data $Data -Before $before -Extension $Extension -Reason $Reason
}

function New-CopyOperation {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination,
        [string]$Reason = ""
    )
    $sourceExists = Test-Path -LiteralPath $Source
    $destinationExists = Test-Path -LiteralPath $Destination
    [ordered]@{
        kind = "file"
        action = "Copy"
        source = $Source
        destination = $Destination
        sourceExists = $sourceExists
        sourceSha256 = if ($sourceExists) { Get-Sha256FileHash -Path $Source } else { $null }
        destinationBefore = [ordered]@{
            existed = $destinationExists
            sha256 = if ($destinationExists) { Get-Sha256FileHash -Path $Destination } else { $null }
        }
        reason = $Reason
    }
}

function Get-OriginalHandlerFromSharedManifest {
    param([Parameter(Mandatory)][string]$Extension)

    $manifestPath = Join-Path $script:BackupRoot "install-manifest.json"
    if (-not (Test-Path -LiteralPath $manifestPath)) {
        return $null
    }

    try {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        $op = @($manifest.plan.operations | Where-Object {
            $_.reason -eq "Save original handler CLSID" -and $_.extension -eq $Extension
        } | Select-Object -First 1)
        if ($op.Count -gt 0) {
            return $op[0].data
        }
    } catch {
        return $null
    }
    return $null
}

function New-InstallPlan {
    param(
        [Parameter(Mandatory)]$Audit,
        [Parameter(Mandatory)][string]$SourceRoot,
        [string]$InstallDir = $script:ProjectInstallDir
    )

    $operations = @()
    $buildArtifacts = @(Test-ProjectBuildArtifacts -SourceRoot $SourceRoot)
    $dllArtifact = $buildArtifacts | Where-Object { (Split-Path -Leaf $_.path) -eq "ImagePhysicalSizeShell.dll" } | Select-Object -First 1
    $dllFileName = "ImagePhysicalSizeShell.dll"
    if ($dllArtifact -and $dllArtifact.sha256) {
        $dllFileName = "ImagePhysicalSizeShell.$($dllArtifact.sha256.Substring(0, 12)).dll"
    }

    foreach ($artifact in $buildArtifacts) {
        $leaf = Split-Path -Leaf $artifact.path
        $destLeaf = if ($leaf -eq "ImagePhysicalSizeShell.dll") { $dllFileName } else { $leaf }
        $dest = Join-Path $InstallDir $destLeaf
        $operations += New-CopyOperation -Source $artifact.path -Destination $dest -Reason "Install project binary"
    }
    $schemaSource = Join-Path $SourceRoot "schema\ImagePhysicalSize.propdesc"
    $operations += New-CopyOperation -Source $schemaSource -Destination (Join-Path $InstallDir "ImagePhysicalSize.propdesc") -Reason "Install property schema"
    $helperScripts = @(
        "ipsconfig.ps1",
        "rollback.ps1",
        "uninstall.ps1",
        "repair.ps1",
        "status.ps1",
        "install-shortcuts.ps1",
        "rollback-emergency.cmd"
    )
    foreach ($scriptName in $helperScripts) {
        $scriptSource = Join-Path $SourceRoot "installer\$scriptName"
        $operations += New-CopyOperation -Source $scriptSource -Destination (Join-Path $InstallDir $scriptName) -Reason "Install helper script"
    }

    $dllPath = Join-Path $InstallDir $dllFileName
    $clsidPath = "SOFTWARE\Classes\CLSID\$script:ProjectClsid"
    $operations += New-PlannedRegistrySetValue -Hive HKLM -Path $clsidPath -Data "ImagePhysicalSizeShell Property Handler" -Reason "COM class friendly name"
    $operations += New-PlannedRegistrySetValue -Hive HKLM -Path "$clsidPath\InprocServer32" -Data $dllPath -Reason "COM in-proc server path"
    $operations += New-PlannedRegistrySetValue -Hive HKLM -Path "$clsidPath\InprocServer32" -Name "ThreadingModel" -Data "Both" -Reason "COM threading model"

    foreach ($extAudit in $Audit.extensions) {
        $ext = $extAudit.extension
        $original = $extAudit.hklmPropertyHandler.data
        if (([string]$original).ToLowerInvariant() -eq $script:ProjectClsid.ToLowerInvariant()) {
            $savedOriginal = $extAudit.privateOriginalHandler.data
            if ($savedOriginal) {
                $original = $savedOriginal
            } else {
                $manifestOriginal = Get-OriginalHandlerFromSharedManifest -Extension $ext
                if ($manifestOriginal) {
                    $original = $manifestOriginal
                }
            }
        }
        $operations += New-PlannedRegistrySetValue -Hive HKLM -Path "$script:PrivateOriginalRoot\$ext" -Name "OriginalClsid" -Data $original -Extension $ext -Reason "Save original handler CLSID"
        $operations += New-PlannedRegistrySetValue -Hive HKLM -Path "$script:PropertyHandlerRoot\$ext" -Data $script:ProjectClsid -Extension $ext -Reason "Register proxy property handler"

        foreach ($list in $extAudit.extensionPropertyLists) {
            if ($list.existed -and ($list.type -eq "String" -or $list.type -eq "ExpandString")) {
                $newValue = Add-ProjectPropertiesToList -Existing ([string]$list.data)
                if ($newValue -ne [string]$list.data) {
                    $operations += New-PlannedRegistrySetValue -Hive HKCR -Path $list.path -Name $list.rawName -Type $list.type -Data $newValue -Extension $ext -Reason "Append project properties to Explorer property list"
                }
            }
        }
    }

    [ordered]@{
        schemaVersion = 1
        project = "ImagePhysicalSizeShell"
        generatedAt = (Get-Date).ToString("o")
        installDir = $InstallDir
        projectClsid = $script:ProjectClsid
        buildArtifacts = $buildArtifacts
        aliasChecks = @(Test-AliasGroups -Audit $Audit)
        operations = $operations
    }
}

function New-RollbackRegistryOperation {
    param(
        [Parameter(Mandatory)]$InstallOperation,
        [Parameter(Mandatory)]$CurrentSnapshot
    )

    $before = $InstallOperation.before
    $installedData = $InstallOperation.data
    $currentMatchesProject = $CurrentSnapshot.existed -and ([string]$CurrentSnapshot.data -eq [string]$installedData)

    $action = "Skip"
    $reason = ""
    $restoreData = $null
    $restoreType = $null

    if (-not $currentMatchesProject) {
        $reason = "Current registry value does not match the value planned by this project; treat as third-party conflict."
    } elseif ($before -and $before.existed) {
        $action = "RestoreValue"
        $reason = "Current value matches project value; restore previous value."
        $restoreData = $before.data
        $restoreType = $before.type
    } else {
        $action = "DeleteValue"
        $reason = "Current value matches project-created value; delete because value did not exist before."
    }

    [ordered]@{
        kind = "registry"
        action = $action
        hive = $InstallOperation.hive
        path = $InstallOperation.path
        name = $InstallOperation.name
        rawName = $InstallOperation.rawName
        extension = $InstallOperation.extension
        current = $CurrentSnapshot
        installedData = $installedData
        before = $before
        restoreType = $restoreType
        restoreData = $restoreData
        conflict = (-not $currentMatchesProject)
        reason = $reason
    }
}

function New-RollbackFileOperation {
    param([Parameter(Mandatory)]$InstallOperation)

    $destinationExists = Test-Path -LiteralPath $InstallOperation.destination
    $currentSha = if ($destinationExists) { Get-Sha256FileHash -Path $InstallOperation.destination } else { $null }
    $matchesProject = $destinationExists -and $InstallOperation.sourceSha256 -and ($currentSha -eq $InstallOperation.sourceSha256)

    $action = "Skip"
    $reason = "Destination file is absent or does not match the project-installed hash."
    if ($matchesProject -and $InstallOperation.destinationBefore.existed) {
        $action = "RestoreFileFromBackup"
        $reason = "Project file is present; restore previous file from backup in a real rollback."
    } elseif ($matchesProject) {
        $action = "DeleteFile"
        $reason = "Project-created file is present; delete in a real rollback."
    }

    [ordered]@{
        kind = "file"
        action = $action
        source = $InstallOperation.source
        destination = $InstallOperation.destination
        destinationExists = $destinationExists
        currentSha256 = $currentSha
        installedSha256 = $InstallOperation.sourceSha256
        before = $InstallOperation.destinationBefore
        conflict = ($destinationExists -and -not $matchesProject)
        reason = $reason
    }
}

function New-RollbackPlanFromManifest {
    param([Parameter(Mandatory)]$Manifest)

    $rollbackOperations = @()
    $installOperations = @($Manifest.plan.operations)
    [array]::Reverse($installOperations)

    foreach ($op in $installOperations) {
        if ($op.kind -eq "registry") {
            $current = Read-RegistryValueSnapshot -Hive $op.hive -Path $op.path -Name $op.rawName -Extension $op.extension
            $rollbackOperations += New-RollbackRegistryOperation -InstallOperation $op -CurrentSnapshot $current
        } elseif ($op.kind -eq "file") {
            $rollbackOperations += New-RollbackFileOperation -InstallOperation $op
        }
    }

    [ordered]@{
        schemaVersion = 1
        project = "ImagePhysicalSizeShell"
        mode = "rollback-plan"
        generatedAt = (Get-Date).ToString("o")
        sourceManifestMode = $Manifest.mode
        operations = $rollbackOperations
        conflicts = @($rollbackOperations | Where-Object { $_.conflict })
    }
}

function Read-ManifestFile {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Manifest not found: $Path"
    }
    Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function New-InstallManifest {
    param(
        [Parameter(Mandatory)]$Audit,
        [Parameter(Mandatory)]$Plan,
        [string]$Mode = "dry-run"
    )

    [ordered]@{
        schemaVersion = 1
        project = "ImagePhysicalSizeShell"
        mode = $Mode
        createdAt = (Get-Date).ToString("o")
        windowsVersion = [Environment]::OSVersion.VersionString
        is64BitOperatingSystem = [Environment]::Is64BitOperatingSystem
        user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
        projectClsid = $script:ProjectClsid
        auditBefore = $Audit
        plan = $Plan
    }
}

function Write-JsonFile {
    param(
        [Parameter(Mandatory)]$Object,
        [Parameter(Mandatory)][string]$Path
    )
    $parent = Split-Path -Parent $Path
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $Object | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Test-InstallPreconditions {
    param(
        [Parameter(Mandatory)]$Plan,
        [switch]$RequireBuiltArtifacts,
        [switch]$RequireElevation
    )

    $failures = @()
    if (-not [Environment]::Is64BitOperatingSystem -or -not [Environment]::Is64BitProcess) {
        $failures += "Requires Windows x64 and a 64-bit PowerShell process."
    }
    if ($RequireElevation -and -not (Test-IsAdministrator)) {
        $failures += "Requires elevated administrator context."
    }
    foreach ($artifact in $Plan.buildArtifacts) {
        if ($RequireBuiltArtifacts -and -not $artifact.exists) {
            $failures += "Missing built artifact: $($artifact.path)"
        }
    }
    foreach ($check in $Plan.aliasChecks) {
        if (-not $check.ok) {
            $failures += "Alias group handlers differ or are missing: $($check.extensions -join ', ')"
        }
    }
    foreach ($op in @($Plan.operations | Where-Object { $_.reason -eq "Save original handler CLSID" })) {
        if (([string]$op.data).ToLowerInvariant() -eq $script:ProjectClsid.ToLowerInvariant()) {
            $failures += "Refusing to save project CLSID as original handler for $($op.extension)."
        }
        if (-not $op.data) {
            $failures += "Missing original handler CLSID for $($op.extension)."
        }
    }
    [ordered]@{
        ok = ($failures.Count -eq 0)
        failures = $failures
    }
}

function Invoke-InstallDryRun {
    param(
        [Parameter(Mandatory)][string]$SourceRoot,
        [Parameter(Mandatory)][string]$OutDir
    )

    $audit = Get-FullAudit
    $plan = New-InstallPlan -Audit $audit -SourceRoot $SourceRoot
    $preconditions = Test-InstallPreconditions -Plan $plan -RequireBuiltArtifacts
    $manifest = New-InstallManifest -Audit $audit -Plan $plan -Mode "dry-run"
    $manifest.preconditions = $preconditions

    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    Write-JsonFile -Object $audit -Path (Join-Path $OutDir "handlers-before.json")
    Write-JsonFile -Object $plan -Path (Join-Path $OutDir "install-plan.json")
    Write-JsonFile -Object $manifest -Path (Join-Path $OutDir "install-manifest.dry-run.json")

    $manifest
}

function Assert-RealInstallAllowed {
    param(
        [string]$Authorization,
        [Parameter(Mandatory)]$Preconditions
    )
    if ($Authorization -ne "APLICAR") {
        throw "Real installation requires -Authorization APLICAR."
    }
    if (-not $Preconditions.ok) {
        throw "Real installation is blocked by failed preconditions: $($Preconditions.failures -join '; ')"
    }
}

function Get-RegistryBaseKey {
    param([Parameter(Mandatory)][ValidateSet("HKLM", "HKCU", "HKCR")][string]$Hive)
    switch ($Hive) {
        "HKLM" { return [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::Registry64) }
        "HKCU" { return [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser, [Microsoft.Win32.RegistryView]::Registry64) }
        "HKCR" { return [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::ClassesRoot, [Microsoft.Win32.RegistryView]::Registry64) }
    }
}

function ConvertTo-RegistryValueKind {
    param([string]$Type)
    switch ($Type) {
        "ExpandString" { [Microsoft.Win32.RegistryValueKind]::ExpandString }
        "DWord" { [Microsoft.Win32.RegistryValueKind]::DWord }
        "QWord" { [Microsoft.Win32.RegistryValueKind]::QWord }
        "MultiString" { [Microsoft.Win32.RegistryValueKind]::MultiString }
        "Binary" { [Microsoft.Win32.RegistryValueKind]::Binary }
        default { [Microsoft.Win32.RegistryValueKind]::String }
    }
}

function Set-RegistryValueStrict {
    param(
        [Parameter(Mandatory)][ValidateSet("HKLM", "HKCU", "HKCR")][string]$Hive,
        [Parameter(Mandatory)][string]$Path,
        [string]$Name = "",
        [string]$Type = "String",
        $Data = $null
    )

    $base = Get-RegistryBaseKey -Hive $Hive
    try {
        $key = $base.CreateSubKey($Path, $true)
        if (-not $key) { throw "Unable to open or create registry key: $Hive\$Path" }
        try {
            $kind = ConvertTo-RegistryValueKind -Type $Type
            $key.SetValue($Name, $Data, $kind)
        } finally {
            $key.Dispose()
        }
    } finally {
        $base.Dispose()
    }
}

function Remove-RegistryValueStrict {
    param(
        [Parameter(Mandatory)][ValidateSet("HKLM", "HKCU", "HKCR")][string]$Hive,
        [Parameter(Mandatory)][string]$Path,
        [string]$Name = ""
    )

    $base = Get-RegistryBaseKey -Hive $Hive
    try {
        $key = $base.OpenSubKey($Path, $true)
        if ($key) {
            try {
                $key.DeleteValue($Name, $false)
            } finally {
                $key.Dispose()
            }
        }
    } finally {
        $base.Dispose()
    }
}

function Remove-ProjectRegistryKeysIfEmpty {
    foreach ($providerPath in @(
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers\.png",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers\.jpg",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers\.jpeg",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers\.tif",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers\.tiff",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\$script:ProjectClsid",
        "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\ImagePhysicalSizeShell"
    )) {
        if (Test-Path -LiteralPath $providerPath) {
            Remove-Item -LiteralPath $providerPath -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

function Export-RegistrySnapshotFiles {
    param(
        [Parameter(Mandatory)][string]$OutDir,
        [Parameter(Mandatory)][string]$Prefix
    )

    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    $targets = @(
        @{ key = "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers"; file = "$Prefix-propertyhandlers.reg" },
        @{ key = "HKLM\SOFTWARE\ImagePhysicalSizeShell"; file = "$Prefix-project.reg" },
        @{ key = "HKLM\SOFTWARE\Classes\CLSID\$script:ProjectClsid"; file = "$Prefix-clsid.reg" },
        @{ key = "HKCR\SystemFileAssociations"; file = "$Prefix-systemfileassociations.reg" }
    )
    foreach ($target in $targets) {
        $path = Join-Path $OutDir $target.file
        $null = & reg.exe export $target.key $path /y 2>$null
    }
}

function Invoke-RegisterSchema {
    param(
        [Parameter(Mandatory)][ValidateSet("register", "unregister")][string]$Action,
        [Parameter(Mandatory)][string]$InstallDir
    )

    $exe = Join-Path $InstallDir "RegisterSchema.exe"
    $schema = Join-Path $InstallDir "ImagePhysicalSize.propdesc"
    if (-not (Test-Path -LiteralPath $exe)) { throw "RegisterSchema.exe not found: $exe" }
    if (-not (Test-Path -LiteralPath $schema)) { throw "Property schema not found: $schema" }

    $output = & $exe $Action $schema --allow-write 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "RegisterSchema $Action failed with exit code $LASTEXITCODE. Output: $output"
    }
    $output
}

function Invoke-InstallOperation {
    param(
        [Parameter(Mandatory)]$Operation,
        [Parameter(Mandatory)][string]$BackupFilesDir
    )

    if ($Operation.kind -eq "file") {
        if (-not $Operation.sourceExists) { throw "Source file missing: $($Operation.source)" }
        $destParent = Split-Path -Parent $Operation.destination
        if ($destParent) { New-Item -ItemType Directory -Force -Path $destParent | Out-Null }
        if ((Test-Path -LiteralPath $Operation.destination) -and $Operation.sourceSha256) {
            $currentHash = Get-Sha256FileHash -Path $Operation.destination
            if ($currentHash -eq $Operation.sourceSha256) {
                return
            }
        }
        if ($Operation.destinationBefore.existed) {
            New-Item -ItemType Directory -Force -Path $BackupFilesDir | Out-Null
            $backupName = ([IO.Path]::GetFileName($Operation.destination) + "." + ([guid]::NewGuid().ToString("N")) + ".bak")
            $backupPath = Join-Path $BackupFilesDir $backupName
            Copy-Item -LiteralPath $Operation.destination -Destination $backupPath -Force
            $Operation.destinationBefore | Add-Member -NotePropertyName backupPath -NotePropertyValue $backupPath -Force
        }
        Copy-Item -LiteralPath $Operation.source -Destination $Operation.destination -Force
        return
    }

    if ($Operation.kind -eq "registry") {
        if ($Operation.action -ne "SetValue") { throw "Unsupported install registry action: $($Operation.action)" }
        Set-RegistryValueStrict -Hive $Operation.hive -Path $Operation.path -Name $Operation.rawName -Type $Operation.type -Data $Operation.data
        return
    }

    throw "Unsupported operation kind: $($Operation.kind)"
}

function Invoke-RollbackOperation {
    param([Parameter(Mandatory)]$Operation)

    if ($Operation.kind -eq "registry") {
        if ($Operation.action -eq "RestoreValue") {
            Set-RegistryValueStrict -Hive $Operation.hive -Path $Operation.path -Name $Operation.rawName -Type $Operation.restoreType -Data $Operation.restoreData
        } elseif ($Operation.action -eq "DeleteValue") {
            Remove-RegistryValueStrict -Hive $Operation.hive -Path $Operation.path -Name $Operation.rawName
        }
        return
    }

    if ($Operation.kind -eq "file") {
        if ($Operation.action -eq "RestoreFileFromBackup") {
            $backupPath = $Operation.before.backupPath
            if (-not $backupPath -or -not (Test-Path -LiteralPath $backupPath)) {
                throw "Missing backup file for restore: $($Operation.destination)"
            }
            Copy-Item -LiteralPath $backupPath -Destination $Operation.destination -Force
        } elseif ($Operation.action -eq "DeleteFile") {
            if (Test-Path -LiteralPath $Operation.destination) {
                Remove-Item -LiteralPath $Operation.destination -Force
            }
        }
        return
    }
}

function Invoke-RollbackFromManifest {
    param(
        [Parameter(Mandatory)]$Manifest,
        [string]$LogPath = ""
    )

    $messages = @()
    if ($Manifest.schemaRegistered) {
        try {
            $messages += Invoke-RegisterSchema -Action unregister -InstallDir $Manifest.plan.installDir
        } catch {
            $messages += "Schema unregister failed during rollback: $($_.Exception.Message)"
        }
    }

    $plan = New-RollbackPlanFromManifest -Manifest $Manifest
    foreach ($op in $plan.operations) {
        if ($op.conflict) {
            $target = if ($op.kind -eq "registry") { "$($op.hive)\$($op.path)" } else { $op.destination }
            $messages += "Skipped conflicted rollback operation: $($op.kind) $target"
            continue
        }
        try {
            Invoke-RollbackOperation -Operation $op
            $messages += "Rolled back: $($op.kind) $($op.action)"
        } catch {
            $messages += "Rollback operation failed: $($_.Exception.Message)"
        }
    }
    Remove-ProjectRegistryKeysIfEmpty

    if ($LogPath) {
        $messages | Set-Content -LiteralPath $LogPath -Encoding UTF8
    }
    $messages
}

function Invoke-RealInstall {
    param(
        [Parameter(Mandatory)][string]$SourceRoot,
        [string]$BackupRoot = $script:BackupRoot
    )

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $backupDir = Join-Path $BackupRoot $timestamp
    New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
    $filesBackupDir = Join-Path $backupDir "files-before"
    $logPath = Join-Path $backupDir "installation.log"

    $audit = Get-FullAudit
    $plan = New-InstallPlan -Audit $audit -SourceRoot $SourceRoot
    $preconditions = Test-InstallPreconditions -Plan $plan -RequireBuiltArtifacts -RequireElevation
    if (-not $preconditions.ok) {
        throw "Real installation is blocked by failed preconditions: $($preconditions.failures -join '; ')"
    }

    $manifest = New-InstallManifest -Audit $audit -Plan $plan -Mode "installed"
    $manifest.preconditions = $preconditions
    $manifest.backupDir = $backupDir
    $manifest.schemaRegistered = $false
    $manifest.appliedOperations = @()

    try {
        Export-RegistrySnapshotFiles -OutDir $backupDir -Prefix "registry-before"
        Write-JsonFile -Object $audit -Path (Join-Path $backupDir "handlers-before.json")
        Write-JsonFile -Object $plan -Path (Join-Path $backupDir "install-plan.json")

        foreach ($op in $plan.operations) {
            Invoke-InstallOperation -Operation $op -BackupFilesDir $filesBackupDir
            $manifest.appliedOperations += $op
            Write-JsonFile -Object $manifest -Path (Join-Path $backupDir "install-manifest.json")
        }

        $schemaOutput = Invoke-RegisterSchema -Action register -InstallDir $plan.installDir
        $manifest.schemaRegistered = $true
        $manifest.schemaRegisterOutput = $schemaOutput

        Export-RegistrySnapshotFiles -OutDir $backupDir -Prefix "registry-after"
        $after = Get-FullAudit
        Write-JsonFile -Object $after -Path (Join-Path $backupDir "handlers-after.json")
        Write-JsonFile -Object $manifest -Path (Join-Path $backupDir "install-manifest.json")
        Copy-Item -LiteralPath (Join-Path $backupDir "install-manifest.json") -Destination (Join-Path $BackupRoot "install-manifest.json") -Force

        "Installation completed at $((Get-Date).ToString('o'))" | Set-Content -LiteralPath $logPath -Encoding UTF8
        return [ordered]@{
            ok = $true
            backupDir = $backupDir
            manifestPath = (Join-Path $backupDir "install-manifest.json")
            sharedManifestPath = (Join-Path $BackupRoot "install-manifest.json")
            schemaRegistered = $true
        }
    } catch {
        $errorMessage = $_.Exception.Message
        $manifest.failure = $errorMessage
        Write-JsonFile -Object $manifest -Path (Join-Path $backupDir "install-manifest.failed.json")
        $rollbackLog = Join-Path $backupDir "automatic-rollback.log"
        Invoke-RollbackFromManifest -Manifest $manifest -LogPath $rollbackLog | Out-Null
        throw "Installation failed and rollback was attempted. Failure: $errorMessage. Backup: $backupDir"
    }
}

Export-ModuleMember -Function `
    Get-FullAudit, `
    New-InstallPlan, `
    New-InstallManifest, `
    Invoke-InstallDryRun, `
    Test-InstallPreconditions, `
    Test-AliasGroups, `
    Add-ProjectPropertiesToList, `
    Assert-RealInstallAllowed, `
    Write-JsonFile, `
    Read-ManifestFile, `
    New-RollbackPlanFromManifest, `
    Invoke-RealInstall, `
    Invoke-RollbackFromManifest, `
    Invoke-RegisterSchema
