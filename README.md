> **AI agent instructions (organization-wide):** Before working on code, issues, or pull requests, read and follow the [organization `AGENTS.md`](https://github.com/atto-lang/.github/blob/main/AGENTS.md), including its **merge-first stealth-development policy**. Honor any repository-local `AGENTS.md`, explicit holds, and GitHub-enforced rules.

# atto-platform.embedded

Embedded AttoLang runtimes and target support for microcontrollers and resource-constrained systems.

## Naming migration and language authority

This repository is maintained at [atto-lang/atto-platform.embedded](https://github.com/atto-lang/atto-platform.embedded); use `https://github.com/atto-lang/atto-platform.embedded.git` for Git remotes and canonical `.atto` source examples. The normative language contract is [atto-spec](https://github.com/atto-lang/atto-spec), with executable guest evidence tracked by [atto-platform-conformance](https://github.com/atto-lang/atto-platform-conformance). Follow current ownership, borrowing, effects, actor, and syntax rules there; do not infer implemented features from a README or unexecuted CI.

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

AttoLang is the canonical project name; new source, examples and documentation use `.atto` and the `atto-lang` repository organization. This README defines scope, not feature readiness.

**Acceptance:** name exact source SHA/tree, runtime/toolchain, positive and negative checks and execution receipt before claiming support. Host-only tests and empty CI steps do not prove guest execution.

## Migration and compatibility guardrails

The canonical AttoLang repository and the archived legacy Oreslang embedded repository have distinct GitHub identities; this is not an identity-preserving transfer. Preserve supported legacy `.ores` fixtures and C ABI/toolchain compatibility until separately tested migrations are available. New language sources use `.atto` and follow the [current grammar](https://github.com/atto-lang/atto-spec/blob/main/spec/01-source-and-syntax.md) and [ownership contract](https://github.com/atto-lang/atto-spec/blob/main/spec/04-ownership-and-memory.md). Target syntax does not establish backend support.

Embedded target constraints, small-footprint runtime integration and native hardware receipts belong here. Full desktop/mobile runtime guarantees and managed cloud operations do not. [atto-co](https://github.com/atto-co) owns hosted control planes, deployment, tenancy and billing; any future carve-out requires separate authorization and review.

For future migrations, document source and target administrators, independent security review, exact commits/trees and repository IDs, access rules, Actions, secrets, apps, deploy keys, PRs, releases, packages, Pages, dependencies and rollback. Verify these after an authorized transfer; temporary CI mirrors are not transfers. Empty or null job steps are NOT_EXECUTED. Host-only checks cannot establish guest parity, zero-GC, AOT or board execution.
