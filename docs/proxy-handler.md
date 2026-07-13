# COM Proxy Property Handler

Phase 4 adds source for the native x64 COM property-handler proxy DLL.

## CLSID

`{D75F9AC7-4664-46F3-A1D4-881140DF6CBE}`

## Implemented interfaces

- `IInitializeWithStream`
- `IPropertyStore`
- `IPropertyStoreCapabilities`
- `IClassFactory`
- standard COM DLL exports:
  - `DllMain`
  - `DllCanUnloadNow`
  - `DllGetClassObject`
  - `DllRegisterServer`
  - `DllUnregisterServer`

`DllRegisterServer` and `DllUnregisterServer` intentionally return `E_NOTIMPL`. Registration must be performed transactionally by the installer after backup and rollback validation, not by ad hoc self-registration.

## Initialization flow

1. Receive the shell-provided `IStream`.
2. Inspect the stream using WIC plus the project metadata parser.
3. Detect the image container format.
4. Map format to canonical extension:
   - PNG -> `.png`
   - JPEG -> `.jpg`
   - TIFF -> `.tif`
5. Read the saved original handler CLSID from:

```text
HKLM\SOFTWARE\ImagePhysicalSizeShell\OriginalHandlers\<extension>
  OriginalClsid = {original-handler-clsid}
```

6. Reject initialization if the saved CLSID is missing or equals the project CLSID.
7. Instantiate the original handler directly by CLSID.
8. Initialize the original handler with a cloned stream.
9. Cache only per-instance computed physical-size data.

## Delegation

`GetCount` returns the original handler's count plus seven project properties.

`GetAt` returns original properties first, then project properties.

`GetValue` returns project calculated values for project keys and delegates every other key.

`SetValue` returns `STG_E_ACCESSDENIED` for project keys and delegates native keys.

`Commit` delegates to the original handler.

`IPropertyStoreCapabilities::IsPropertyWritable` returns `S_FALSE` for project keys and delegates native keys when the original handler supports `IPropertyStoreCapabilities`.

## Stream-only alias limitation

`IInitializeWithStream` does not provide the file path or extension. The proxy detects PNG/JPEG/TIFF from bytes. For aliases such as `.jpg` versus `.jpeg`, and `.tif` versus `.tiff`, byte-level detection cannot identify which extension caused the shell to instantiate the handler.

Therefore the installer must enforce:

- `.jpg` and `.jpeg` original handler CLSIDs must match before installing both;
- `.tif` and `.tiff` original handler CLSIDs must match before installing both.

If they differ, installation must abort for the ambiguous alias group and report the risk. This avoids delegating to the wrong original handler.

## Safety status

This source has not been compiled in the current environment because MSVC/CMake/Windows SDK are unavailable. The DLL has not been registered or loaded by Explorer.

