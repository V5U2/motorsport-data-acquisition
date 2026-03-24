# Agent Notes

- After significant firmware changes, run the available host-side verification in `./scripts/run-host-tests.sh`. If PlatformIO is available, also run `pio run` so the embedded build stays healthy.
- After feature, configuration, hardware, or behavior changes, update `README.md` and `docs/hardware-setup.md` so the documentation stays aligned with the current firmware and wiring expectations.
- Keep the sensor model extensible. Sensor definitions live in `include/AppConfig.h` via `kSensorConfigs`; when adding channels, prefer extending that configuration rather than reintroducing hardcoded per-sensor paths in the firmware, web UI, CSV logging, or dashboard code.
- Keep hardware-sensitive defaults aligned with the docs. If pin mappings, display controller assumptions, ADC/shunt behavior, Wi-Fi behavior, or BOM guidance change, update the relevant config headers and documentation together.
- Add or update automated tests for new logic and behavior changes so coverage keeps pace with the firmware. Hardware-independent logic should stay covered by the host test suite when practical.
- Keep GitHub release automation aligned with repo changes. If versioning, release tagging, firmware artifacts, or published release behavior changes, update the workflows under `.github/workflows/` and document the release process in `README.md`.
- Use Conventional Commits for commit messages: `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `chore:`, `perf:`, `build:`, and `ci:`. Use `!` or `BREAKING CHANGE:` for breaking changes so `release-please` can classify them correctly.
- Use SemVer tags and GitHub Release names in the form `vX.Y.Z`.
