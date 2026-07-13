# Rollback Simulation

Phase 6 extends the dry-run installer with manifest-based rollback planning.

## Goal

Rollback must never blindly overwrite system state. It should restore a value only when the current value still matches the value installed by this project.

## Install operation metadata

Dry-run registry operations now include:

- target hive/path/name;
- planned installed data;
- original value snapshot before installation;
- extension and reason.

Dry-run file operations include:

- source and destination;
- source hash when available;
- destination existence/hash before installation.

## Rollback rule

For each registry operation in reverse order:

1. Read the current registry value.
2. Compare current value with the value installed/planned by this project.
3. If current value differs, mark conflict and skip.
4. If current matches project value and the value existed before, plan `RestoreValue`.
5. If current matches project value and the value did not exist before, plan `DeleteValue`.

For each file operation in reverse order:

1. Hash the current destination if it exists.
2. Compare it to the project-installed hash.
3. If it differs, mark conflict and skip.
4. If it matches and a previous file existed, plan restore from backup.
5. If it matches and no previous file existed, plan delete.

## Current simulation result

Using the dry-run install manifest on a machine where nothing was installed:

- 37 rollback operations were analyzed;
- 37 were skipped;
- 33 registry operations were marked as conflicts because current values do not match project-installed values;
- 0 restore operations were planned;
- 0 delete operations were planned.

This is the expected safe behavior for a dry-run manifest that was never applied.

