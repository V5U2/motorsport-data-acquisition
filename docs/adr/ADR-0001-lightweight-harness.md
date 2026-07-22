# ADR-0001: Lightweight Harness for Repo Contracts

## Status

Accepted

## Context

This repository already has useful harness primitives:
- a short [`AGENTS.md`](../../AGENTS.md)
- a human entrypoint in [`README.md`](../../README.md)
- detailed hardware documentation in [`docs/hardware-setup.md`](../hardware-setup.md)
- host-side tests in [`tests/logic_tests.cpp`](../../tests/logic_tests.cpp) via [`scripts/run-host-tests.sh`](../../scripts/run-host-tests.sh)
- CI workflows under [`.github/workflows/`](../../.github/workflows/)

The failure mode is not absence of documentation. It is drift between duplicated human-readable facts and stale workflow assumptions.

## Decision

Adopt a lightweight embedded-firmware harness centered on:
- explicit source ownership
- one contract document
- one contract-check script
- one repo verification wrapper
- reuse of existing CI workflows

## Drivers

- reduce drift between `platformio.ini`, workflows, and docs
- keep process overhead low for a small firmware project
- improve agent and reviewer clarity with minimal repo surface increase

## Alternatives Considered

### Docs-only cleanup

Pros:
- quick to land
- low tooling cost

Cons:
- leaves drift enforcement manual
- does not catch stale workflow/env assumptions

### Full harness framework

Pros:
- strongest formalization
- broader agent-facing structure

Cons:
- too heavy for current repo size
- increases documentation/process entropy before fixing ownership

## Why This Was Chosen

It addresses the actual repo problem with the smallest durable layer that can be mechanically verified.

## Consequences

### Positive

- fewer mirrored facts
- clearer ownership boundaries
- local and CI verification become more consistent

### Negative

- a small amount of shell scripting and documentation maintenance is added
- checks can become brittle if they are allowed to depend on prose instead of owner files

## Follow-ups

- only add deeper architecture docs if project invariants become non-obvious from code
- only add richer automation if repo complexity grows enough to justify it
