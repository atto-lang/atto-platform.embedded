# atto-platform.embedded

Embedded AttoLang runtimes and target support for microcontrollers and resource-constrained systems.

## Scope

Microcontroller toolchains, memory constraints, embedded actor lifecycle, and target-level test fixtures.

## Portability contract

The AttoLang language contract must remain separate from target-specific capabilities. Document supported toolchains, devices, platform APIs, memory limits, runtime constraints, and unsupported features explicitly.

## Verification

Where feasible, use real devices or representative emulators in addition to host-side tests. Record toolchain versions, target profiles, exact commit SHAs, and execution evidence. A successful build alone is not proof of runtime semantics.

## Development status

This is a statement of intended project responsibilities. Consult source files and CI for verified implementation or release status; do not assume all targets are supported.

## Related projects

- [AttoLang](https://github.com/atto-lang)
- [Target profiles](https://github.com/atto-lang/atto-target-profiles)
- [Platform conformance](https://github.com/atto-lang/atto-platform-conformance)

## Repository ownership and contribution routing

**Belongs here:** MCU toolchains, bare-metal integration, bounded-memory runtimes and device-level actor execution fixtures.

**Belongs elsewhere:** Language specification in atto-spec; portable runtime ABI in atto-common-source; HDL synthesis in atto-source.hdl.

**Security and portability boundary:** Explicitly declare heap/stack, interrupt, scheduling and memory budgets; fail closed on unsupported I/O, cancellation and real-time guarantees.

AttoLang is the public name; preserve existing Oreslang/.ores compatibility and use canonical .atto for new portable library source. This README defines scope, not feature readiness.

**Acceptance:** name exact source SHA/tree, runtime/toolchain, positive and negative checks and execution receipt before claiming support. Host-only tests and empty CI steps do not prove guest execution.
