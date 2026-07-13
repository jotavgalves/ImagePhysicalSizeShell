# Architecture

## Goal

ImagePhysicalSizeShell adds read-only calculated image properties to the original Windows Explorer:

- `ImagePhysicalSizeShell.PhysicalWidthCm`
- `ImagePhysicalSizeShell.PhysicalHeightCm`
- `ImagePhysicalSizeShell.PhysicalSizeCm`
- `ImagePhysicalSizeShell.EmbeddedDpiX`
- `ImagePhysicalSizeShell.EmbeddedDpiY`
- `ImagePhysicalSizeShell.DpiSource`
- `ImagePhysicalSizeShell.DpiStatus`

The native Windows property surface must remain intact. `System.Image.Dimensions` remains the Windows pixel dimension property.

## Non-destructive proxy design

Windows registers property handlers per extension under the Property System registry area. The production handler must be installed as a proxy/decorator:

1. Audit the current handler CLSID for each supported extension.
2. Save the exact original CLSID in a manifest and in a project-private key.
3. Register the project CLSID only after backup and validation.
4. During `IInitializeWithStream`, instantiate the saved original handler directly by CLSID.
5. Forward native properties to the original handler.
6. Append only project-owned calculated properties.
7. Never look up the "current" extension mapping after the proxy is installed, avoiding recursion.

If an original handler cannot be instantiated or cannot preserve required behavior, that extension must not be installed.

## Phase 0 boundary

Phase 0 is read-only:

- inspect Windows/toolchain availability;
- create source tree;
- document the proxy architecture;
- implement `ipsdiag handlers`;
- implement read-only registry backup;
- produce an audit report.

Phase 0 does not:

- write to HKLM, HKCU or HKCR;
- copy files to Program Files;
- register COM;
- register `.propdesc`;
- restart Explorer.

## Handler responsibilities

The production COM DLL will implement:

- `IUnknown`;
- `IInitializeWithStream`;
- `IPropertyStore`;
- `IPropertyStoreCapabilities`;
- `IClassFactory`;
- standard COM DLL exports.

The DLL will keep only per-instance cache for the initialized file. It will use WIC metadata readers and stream clones where appropriate so the original handler and the project parser do not interfere with each other's stream position.

## Delegation contract

`GetCount` returns original properties followed by project properties, with duplicate property keys removed.

`GetAt` is deterministic: original order first, project properties second.

`GetValue` returns project values for project keys and delegates all other keys.

`SetValue` rejects project keys and delegates non-project keys.

`Commit` delegates only. Calculated centimeters are never persisted into the image file.

`IPropertyStoreCapabilities::IsPropertyWritable` returns `S_FALSE` for project keys and delegates native keys.

## Stream-only alias constraint

Because the handler uses `IInitializeWithStream`, it does not receive a path or extension. The proxy can detect PNG/JPEG/TIFF from bytes, but it cannot distinguish `.jpg` from `.jpeg` or `.tif` from `.tiff`. Installation must verify that aliases in each group share the same original handler CLSID. If not, installation must abort that group rather than risk incorrect delegation.

## DPI metadata precedence

The planned precedence is:

1. EXIF/TIFF explicit `XResolution`, `YResolution`, `ResolutionUnit`;
2. JFIF APP0 explicit density with unit inch or centimeter;
3. PNG `pHYs` with meter unit;
4. no embedded physical resolution.

This order is intentionally conservative and must be validated against fixtures before installation support is enabled.

## Registry areas audited

Phase 0 audits the supported extensions:

- `.png`
- `.jpg`
- `.jpeg`
- `.tif`
- `.tiff`

For each extension it reads:

- `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\<extension>`;
- `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\<extension>`;
- `HKCR\<extension>`;
- `HKCR\<extension>\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}`;
- `HKCR\SystemFileAssociations\<extension>`;
- `HKCR\SystemFileAssociations\image`;
- the extension default ProgID when present.

The values of interest include `FullDetails`, `PreviewDetails`, `PreviewTitle`, `InfoTip`, `TileInfo`, `ExtendedTileInfo`, default value, `Content Type`, `PerceivedType` and property-handler CLSIDs.

## Rollback principle

Rollback must compare current values with the values installed by this project before restoring. If another product changed a value after installation, rollback must report a conflict and leave that value untouched.
