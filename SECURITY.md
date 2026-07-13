# Security and Safety Model

ImagePhysicalSizeShell is intended to be loaded by Explorer. The production DLL must therefore be small, deterministic and conservative.

Hard rules:

- no network access from the shell DLL;
- no child processes from the shell DLL;
- no UI from the shell DLL;
- no image rewriting for calculated properties;
- no fallback to 72, 96, 150 or 300 DPI when physical metadata is absent;
- no registry writes during Phase 0;
- no COM registration before explicit user authorization using `APLICAR`.

Installation and rollback must be transactional. Any future installer must save a machine-readable manifest before changing registry values and must restore only values it owns.

