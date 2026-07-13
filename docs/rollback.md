# Rollback

Rollback must be independent of the COM DLL and diagnostic executable.

The emergency rollback path will depend only on:

- PowerShell;
- native Windows tools;
- manifest and registry backup in ProgramData.

It must never delete backups automatically.

