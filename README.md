# ImagePhysicalSizeShell

Native Windows 11 x64 shell extension project for calculated image physical-size properties.

The extension will add project-owned read-only properties such as physical width/height in centimeters, embedded DPI and DPI source. It must not replace `System.Image.Dimensions` and must preserve the original Windows property handlers for PNG, JPEG and TIFF.

## Phase 1 status

This repository currently implements the safe audit and parsing phases:

- repository structure;
- architecture notes;
- stable property keys and draft property schema;
- `ipsdiag handlers`;
- `ipsdiag status`;
- `ipsdiag backup-registry <directory>`;
- `ipsdiag inspect <image>`;
- physical-size calculation;
- bounded metadata scanners for PNG `pHYs`, JPEG JFIF/EXIF and TIFF IFD;
- property schema validation tooling;
- `RegisterSchema.exe` source for future schema registration/unregistration;
- native COM proxy handler source;
- dry-run transactional installer planning;
- manifest-based rollback simulation;
- PowerShell status/audit helpers.

The project still does not register COM, does not register the property schema, does not modify file associations and does not restart Explorer.

## Build

From a Visual Studio 2022 Developer PowerShell:

```powershell
.\build.ps1 -Configuration Release
```

This requires Windows x64, MSVC, CMake and the Windows SDK.

To build and run unit tests:

```powershell
.\test.ps1 -Configuration Release
```

To check whether the local machine is ready for Phase 2 without changing system state:

```powershell
.\tools\phase2-preflight.ps1 -OutDir .\phase2-preflight
```

To validate the `.propdesc` structure without registering it:

```powershell
.\tools\validate-propdesc.ps1 -Path .\schema\ImagePhysicalSize.propdesc
```

To generate an install plan without changing the machine:

```powershell
.\installer\install.ps1 -DryRun -OutDir .\install-dry-run
```

## Safe audit

```powershell
.\installer\status.ps1 -OutDir .\audit
```

or, after building:

```powershell
.\build\Release\ipsdiag.exe handlers
.\build\Release\ipsdiag.exe backup-registry .\audit
```
