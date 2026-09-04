# Firmware releases and rollback

Firmware releases are immutable builds of an existing `vMAJOR.MINOR.PATCH` tag. Release Please creates the version commit, tag, changelog and GitHub Release from protected `main`; the tag-triggered `release-firmware` workflow builds and attaches the board-specific artifacts.

## Promotion

1. Merge conventional commits to `main` only after required verification passes.
2. Review the Release Please PR. Confirm its version and changelog, then merge it through normal protection.
3. Confirm the tag points at that merge commit and the `release-firmware` workflow succeeds.
4. Download `SHA256SUMS` and `build-metadata.json` from the GitHub Release. Run `sha256sum --check SHA256SUMS` in the download directory.
5. Confirm the metadata tag and commit match the GitHub Release, then select the artifact whose environment, flash size, and partition layout match the physical logger.
6. Install on a non-race logger first and run the commissioning checks in [Hardware Setup](hardware-setup.md).

The release job validates the tag/checkout identity, verifies every prepared checksum, refuses to overwrite an existing asset, downloads the exact published assets, verifies their checksums again, and compares them byte-for-byte with the build outputs. A failure therefore remains visible and cannot claim success after a partial or mismatched publication.

## Manual or emergency publication

Use the `release-firmware` workflow's **Run workflow** action only for an existing SemVer tag. Enter the complete tag such as `v0.2.1`; the job checks out and validates that tag rather than the selected branch. Do not create a release from an untagged branch or upload locally built binaries to an existing release.

If a run fails before assets are uploaded, correct the workflow defect and rerun it for the same tag. If any asset was uploaded, do not replace it: preserve the failed release as incident evidence, create a new patch tag after the correction, and publish new immutable artifacts.

## Rollback rehearsal

Rollback is a forward operational action using the prior known-good release; tags and releases are never moved.

1. Record the current logger version, board/flash layout, and queue diagnostics. Preserve or drain queued telemetry before changing the partition.
2. Download the prior release's `SHA256SUMS`, metadata, and matching firmware artifact. Verify all checksums and confirm its target/layout.
3. For NodeMCU, upload the matching application binary with the documented serial or OTA procedure. For ESP32, use the application binary only when the installed partition layout is already compatible; otherwise use the matching factory image after explicitly accepting that a full flash operation can erase settings and queued data.
4. Re-run the commissioning checklist, verify the local version and live sequence, and confirm ingestion in the telemetry app.
5. Record the rollback release, device ID, reason, verification result, and whether queue data was preserved.

For a rehearsal, perform these steps on a non-race logger: install the new release, verify it, reinstall the immediately preceding known-good release, verify it, then reinstall the new release. A production release is not considered track-ready until that evidence exists for each supported physical target in use.
