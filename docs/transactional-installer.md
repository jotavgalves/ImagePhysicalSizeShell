# Transactional Installer

Phase 5 adds the PowerShell installation planning framework.

## Current status

The installer is not production-enabled. It supports:

- read-only audit;
- dry-run install plan;
- dry-run uninstall plan;
- dry-run repair plan;
- dry-run rollback plan;
- precondition checks;
- alias-group validation;
- manifest generation.

Real installation remains blocked until:

- Release x64 binaries exist;
- unit tests pass;
- rollback is validated;
- the user provides `-Authorization APLICAR`;
- the script is extended to execute operations with inverse-operation tracking.

## Safety gates

`install.ps1 -Install` refuses to proceed unless:

- `-Authorization APLICAR` is provided;
- required build artifacts exist:
  - `ImagePhysicalSizeShell.dll`;
  - `ipsdiag.exe`;
  - `RegisterSchema.exe`;
- alias groups have matching original handler CLSIDs:
  - `.jpg` and `.jpeg`;
  - `.tif` and `.tiff`;
- the process is elevated.

Even after these gates, the current phase still throws before applying writes because real transaction execution is intentionally not enabled.

## Dry-run output

`install.ps1 -DryRun` writes:

- `handlers-before.json`;
- `install-plan.json`;
- `install-manifest.dry-run.json`.

The plan includes file copy operations and registry operations that a future real installer would perform.

## Planned registry areas

The future installer will write only after backup and authorization:

- `HKLM\SOFTWARE\Classes\CLSID\{D75F9AC7-4664-46F3-A1D4-881140DF6CBE}`;
- `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\<extension>`;
- `HKLM\SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers\<extension>`;
- selected property-list values under `HKCR\SystemFileAssociations\<extension>`.

Property lists are extended by appending project properties and preserving existing values.

## Rollback principle

The future real transaction executor must maintain a LIFO operation stack. Each write operation needs an inverse operation created from the backup snapshot. During uninstall or rollback, a value should be restored only when the current value still matches the value installed by this project.

Phase 6 adds dry-run rollback planning from a manifest. See `rollback-simulation.md`.
