# Phase 2 Build Validation

Phase 2 validates the C++ code on Windows x64 with MSVC, CMake and the Windows SDK.

## Local prerequisites

Install Visual Studio 2022 or Build Tools with:

- Desktop development with C++;
- MSVC v143 x64/x86 build tools;
- Windows 10/11 SDK;
- CMake tools for Windows.

## Local commands

```powershell
.\tools\phase2-preflight.ps1 -OutDir .\phase2-preflight
.\test.ps1 -Configuration Release
```

`test.ps1` configures, builds and runs `ctest`.

## Current local environment result

The current machine used for this phase does not have:

- `cmake`;
- `ctest`;
- `cl.exe`;
- `msbuild`;
- `ninja`;
- Visual Studio setup registry entries;
- Windows Kits installed-root registry entries.

Therefore Release x64 compilation and unit-test execution could not be completed here.

## CI option

The repository includes `.github/workflows/windows-build.yml`, which builds and tests on `windows-latest`. This is a validation aid only. It does not install the shell extension and does not write to Windows Explorer registry areas.

