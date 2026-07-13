# Property Schema

The project defines seven read-only calculated properties in the `ImagePhysicalSizeShell` canonical namespace.

## Permanent format ID

`{7A4E8B66-6C8A-421D-9868-42F50F9312B4}`

## Properties

| Canonical name | propID | Type | Purpose |
| --- | ---: | --- | --- |
| `ImagePhysicalSizeShell.PhysicalWidthCm` | 2 | `Double` | Numeric physical width in centimeters. |
| `ImagePhysicalSizeShell.PhysicalHeightCm` | 3 | `Double` | Numeric physical height in centimeters. |
| `ImagePhysicalSizeShell.PhysicalSizeCm` | 4 | `String` | Friendly display string, for example `42,47 x 42,47 cm`. |
| `ImagePhysicalSizeShell.EmbeddedDpiX` | 5 | `Double` | Embedded horizontal DPI. |
| `ImagePhysicalSizeShell.EmbeddedDpiY` | 6 | `Double` | Embedded vertical DPI. |
| `ImagePhysicalSizeShell.DpiSource` | 7 | `String` | Metadata source selected by the parser. |
| `ImagePhysicalSizeShell.DpiStatus` | 8 | `String` | Human-readable DPI detection status. |

## Registration

The official Windows APIs are `PSRegisterPropertySchema` and `PSUnregisterPropertySchema`. Registration is machine-wide, requires privileges, should use an absolute local path, and the `.propdesc` file should live in an install directory readable by all users, typically under Program Files.

This project includes `RegisterSchema.exe` source with three verbs:

```powershell
RegisterSchema.exe validate C:\Program Files\ImagePhysicalSizeShell\ImagePhysicalSize.propdesc
RegisterSchema.exe register C:\Program Files\ImagePhysicalSizeShell\ImagePhysicalSize.propdesc --allow-write
RegisterSchema.exe unregister C:\Program Files\ImagePhysicalSizeShell\ImagePhysicalSize.propdesc --allow-write
```

The write verbs deliberately require `--allow-write` so accidental manual execution fails closed.

Phase 3 does not execute registration. Registration remains blocked until the project compiles, tests pass, rollback is validated, and the user explicitly authorizes installation.

## Offline validation

Use:

```powershell
.\tools\validate-propdesc.ps1 -Path .\schema\ImagePhysicalSize.propdesc -OutFile .\schema-validation.json
```

This validation checks project invariants but does not replace a real `PSRegisterPropertySchema` validation on a compiler-enabled Windows machine.

