param(
    [ValidateSet("show", "enable", "disable", "fallback-dpi", "decimals", "defaults")]
    [string]$Command = "show",
    [string]$Name = "",
    [string]$Value = ""
)

$ErrorActionPreference = "Stop"

$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$settingsPath = "HKCU:\Software\ImagePhysicalSizeShell\Settings"

$fields = [ordered]@{
    "PhysicalSizeCm" = "ShowPhysicalSizeCm"
    "PhysicalWidthCm" = "ShowPhysicalWidthCm"
    "PhysicalHeightCm" = "ShowPhysicalHeightCm"
    "EmbeddedDpiX" = "ShowEmbeddedDpiX"
    "EmbeddedDpiY" = "ShowEmbeddedDpiY"
    "DpiSource" = "ShowDpiSource"
    "DpiStatus" = "ShowDpiStatus"
}

$aliases = @{
    "tamanho" = "PhysicalSizeCm"
    "tamanhofisico" = "PhysicalSizeCm"
    "tamanhofísico" = "PhysicalSizeCm"
    "largura" = "PhysicalWidthCm"
    "largurafisica" = "PhysicalWidthCm"
    "largurafísica" = "PhysicalWidthCm"
    "altura" = "PhysicalHeightCm"
    "alturafisica" = "PhysicalHeightCm"
    "alturafísica" = "PhysicalHeightCm"
    "dpix" = "EmbeddedDpiX"
    "dpiy" = "EmbeddedDpiY"
    "origem" = "DpiSource"
    "origemresolucao" = "DpiSource"
    "origemresolução" = "DpiSource"
    "status" = "DpiStatus"
}

function Ensure-Key {
    if (-not (Test-Path -LiteralPath $settingsPath)) {
        New-Item -Path $settingsPath -Force | Out-Null
    }
}

function Set-Dword {
    param([string]$Property, [int]$Data)
    Ensure-Key
    New-ItemProperty -LiteralPath $settingsPath -Name $Property -Value $Data -PropertyType DWord -Force | Out-Null
}

function Resolve-Field {
    param([string]$InputName)
    if (-not $InputName) {
        throw "Informe o campo. Exemplo: .\ipsconfig.ps1 enable PhysicalSizeCm"
    }
    if ($fields.Contains($InputName)) {
        return $InputName
    }
    $key = ($InputName -replace "[^A-Za-z0-9]", "").ToLowerInvariant()
    if ($aliases.ContainsKey($key)) {
        return $aliases[$key]
    }
    throw "Campo desconhecido: $InputName. Use: $($fields.Keys -join ', ')"
}

function Get-SettingValue {
    param([string]$Property, [int]$Default)
    if (-not (Test-Path -LiteralPath $settingsPath)) {
        return $Default
    }
    $item = Get-ItemProperty -LiteralPath $settingsPath -ErrorAction SilentlyContinue
    if ($null -eq $item) {
        return $Default
    }
    $prop = $item.PSObject.Properties[$Property]
    if ($null -eq $prop -or $null -eq $prop.Value) {
        return $Default
    }
    return [int]$prop.Value
}

function Show-Settings {
    $defaults = @{
        ShowPhysicalSizeCm = 1
        ShowPhysicalWidthCm = 0
        ShowPhysicalHeightCm = 0
        ShowEmbeddedDpiX = 0
        ShowEmbeddedDpiY = 0
        ShowDpiSource = 1
        ShowDpiStatus = 1
        FallbackDpiEnabled = 0
        FallbackDpi = 72
        MaxDecimalPlaces = 2
        TrimTrailingZeros = 1
    }

    Write-Host "ImagePhysicalSizeShell - configuração do usuário"
    Write-Host "Chave: HKCU\Software\ImagePhysicalSizeShell\Settings"
    Write-Host ""
    foreach ($field in $fields.GetEnumerator()) {
        $enabled = Get-SettingValue -Property $field.Value -Default $defaults[$field.Value]
        Write-Host ("{0,-18} {1}" -f $field.Key, $(if ($enabled) { "ligado" } else { "desligado" }))
    }
    Write-Host ""
    Write-Host ("DPI inferido      {0}" -f $(if (Get-SettingValue -Property "FallbackDpiEnabled" -Default 0) { "ligado" } else { "desligado" }))
    Write-Host ("Valor do DPI      {0}" -f (Get-SettingValue -Property "FallbackDpi" -Default 72))
    Write-Host ("Casas decimais    {0}" -f (Get-SettingValue -Property "MaxDecimalPlaces" -Default 2))
    Write-Host ("Remover zeros     {0}" -f $(if (Get-SettingValue -Property "TrimTrailingZeros" -Default 1) { "ligado" } else { "desligado" }))
}

switch ($Command) {
    "show" {
        Show-Settings
    }
    "enable" {
        $field = Resolve-Field -InputName $Name
        Set-Dword -Property $fields[$field] -Data 1
        Show-Settings
    }
    "disable" {
        $field = Resolve-Field -InputName $Name
        Set-Dword -Property $fields[$field] -Data 0
        Show-Settings
    }
    "fallback-dpi" {
        if ($Name -eq "on") {
            $dpi = if ($Value) { [int]$Value } else { 72 }
            if ($dpi -lt 1 -or $dpi -gt 100000) {
                throw "DPI inferido fora do intervalo permitido."
            }
            Set-Dword -Property "FallbackDpiEnabled" -Data 1
            Set-Dword -Property "FallbackDpi" -Data $dpi
        } elseif ($Name -eq "off") {
            Set-Dword -Property "FallbackDpiEnabled" -Data 0
        } else {
            throw "Use: .\ipsconfig.ps1 fallback-dpi on 72  ou  .\ipsconfig.ps1 fallback-dpi off"
        }
        Show-Settings
    }
    "decimals" {
        if ($Name -eq "") {
            throw "Use: .\ipsconfig.ps1 decimals 2"
        }
        $places = [int]$Name
        if ($places -lt 0 -or $places -gt 6) {
            throw "Use entre 0 e 6 casas decimais."
        }
        Set-Dword -Property "MaxDecimalPlaces" -Data $places
        Set-Dword -Property "TrimTrailingZeros" -Data 1
        Show-Settings
    }
    "defaults" {
        Ensure-Key
        Remove-Item -LiteralPath $settingsPath -Recurse -Force
        Show-Settings
    }
}
