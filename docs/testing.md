# Testing

The current project verifies read-only registry inspection and provides C++ unit-test sources for physical-size calculation and metadata parsing.

Future phases must add:

- unit tests for DPI and centimeter calculations;
- WIC metadata fixtures for PNG/JPEG/TIFF;
- shell integration tests;
- stress tests with at least 1,000 images;
- rollback tests for every installation checkpoint.

Build validation is documented in `phase2-build-validation.md`.
