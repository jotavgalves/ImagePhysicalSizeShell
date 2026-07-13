# Installation

Installation is intentionally not enabled yet.

Future installation must require an explicit authorization step and must run transactionally:

1. verify elevation;
2. verify x64 Windows;
3. audit original handlers;
4. validate original handler instantiation;
5. create backup and manifest;
6. copy files;
7. register COM;
8. register property schema by official API;
9. preserve and extend Explorer property lists;
10. register proxy one extension at a time;
11. run sanity checks;
12. rollback on any failure.

The schema helper is `RegisterSchema.exe`. Its write verbs require `--allow-write`, and installer scripts must still require the outer project authorization flow before calling it.

See `transactional-installer.md` for the Phase 5 dry-run installer model and safety gates.
