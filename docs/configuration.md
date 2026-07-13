# Configuração por usuário

As opções de exibição são lidas por arquivo, uma vez por instância do handler, em:

```text
HKCU\Software\ImagePhysicalSizeShell\Settings
```

Isso permite alterar preferências do usuário sem modificar os handlers globais em `HKLM`.

## Padrão

- `PhysicalSizeCm`: ligado
- `PhysicalWidthCm`: desligado
- `PhysicalHeightCm`: desligado
- `EmbeddedDpiX`: desligado
- `EmbeddedDpiY`: desligado
- `DpiSource`: ligado
- `DpiStatus`: ligado
- DPI inferido: desligado
- valor do DPI inferido: 72
- casas decimais: 2
- remover zeros finais: ligado

## Comandos

Mostrar configuração:

```powershell
.\ipsconfig.ps1 show
```

Ativar ou desativar campo:

```powershell
.\ipsconfig.ps1 enable PhysicalSizeCm
.\ipsconfig.ps1 disable EmbeddedDpiX
```

Ativar inferência de 72 DPI quando não houver DPI físico real:

```powershell
.\ipsconfig.ps1 fallback-dpi on 72
```

Desativar inferência de DPI:

```powershell
.\ipsconfig.ps1 fallback-dpi off
```

Configurar casas decimais:

```powershell
.\ipsconfig.ps1 decimals 2
```

Restaurar padrões:

```powershell
.\ipsconfig.ps1 defaults
```

## Observações

O DPI inferido nunca é tratado como DPI embutido real. Quando ativo, a origem passa a ser `DPI inferido` e o status também informa `DPI inferido`.

Campos desativados deixam de ser enumerados pelo handler. Se o Explorer pedir diretamente um campo desativado por causa de cache ou lista de propriedades já existente, o handler retorna valor vazio.
