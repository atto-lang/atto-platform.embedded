# Experimental Cortex-M3 static actor runtime

The compiler's source-generated `oreslang_actor_<Type>_init` and
`oreslang_actor_<Type>_on_message` functions run with a statically allocated
C adapter on Zephyr. The admitted `.ores` bootstrap uses `spawn`, `send`
and `stop`; `stop` drains a bounded FIFO for each actor on one cooperative
carrier. All actor state and queue storage is statically allocated.

## Bounded execution contract

- Eight actor slots and at most eight source bootstrap spawns per execution.
- Eight scalar i64 state fields, eight generation-fenced actor capability slots,
  and 16 i64 messages per mailbox.
- Bounded direct `send` calls enqueue without blocking. A handler may enqueue
  to a different live actor without executing that actor inline. Self-send from
  a handler and sends from constructors fail closed so a stop-drain loop cannot
  be extended indefinitely from inside a turn. Strict overflow sets a sticky
  failure flag. `oreslang_source_actor_try_send` instead returns 0 for a full
  otherwise-valid target so future generated code can yield/retry without
  unbounded queue growth. A stop drains queued messages and retires its actor.
- Generation-tagged opaque actor references prevent stale sends after a
  slot has been recycled within the same bootstrap.
- All actors execute on a single thread, no guest suspension or concurrent
  handler execution. The only handler input/output is a scalar i64.
- No dynamic allocation exists in the static actor adapter. This does not
  prove the entire Zephyr kernel, toolchain, or firmware never allocates.
- `ores_embedded_actors_reset` is a **boot-only** initializer. Once any
  actor reference has been issued, reset permanently fails closed and sets
  a sticky fault; only a device reboot starts a new actor session. Reusing
  fixed reference addresses between sessions would otherwise resurrect stale
  capabilities.
- Generated field initialization is allowed only during actor construction;
  duplicate/missing initialization and writes to immutable fields fail closed.
  Retiring an actor scrubs its private fields and mailbox, retaining only
  its final scalar result and delivery count in the original immutable ref.
- For each source bootstrap, the compiler statically proves that no mailbox
  receives more than 16 pending messages before a stop; the runtime still
  independently checks capacity at each send.

## Current language boundary

Source `main` can spawn, send literal messages and stop admitted actors.
The generated functions use the existing host actor field ABI, not a new
Oreslang actor syntax. Source actor handlers may update their own private
i64 fields and return an i64. The **runtime ABI now admits handler-to-other-actor enqueue** through bounded
capability slots. Spawn copies only a target slot plus generation; an actor
handler never needs a raw ActorRef pointer in scalar state. The freestanding
C ABI stand-in exercises this capability path and overwrites the caller-owned
pointer array after spawn to prove the runtime copied identity. LLVM PR #943
implements the matching hosted OresVM capability-slot prerequisite. Real
`.ores` source still lacks an admitted cross-backend spelling that binds an
ActorRef capability into actor state, so source-level actor-to-actor application
messaging is not yet proved. Await, supervision and groups remain outside this
embedded profile.

## Tests and negative cases

`tests/test_embedded_static_actors.c` covers enqueue/drain, state persistence,
immutable postmortem telemetry, double stop, mailbox capacity, stale pointer
after slot reuse, illegal reset, duplicate/missing field initialization,
and forbidden field writes during message handling. Every fault scenario runs
in a fresh process to avoid unsafe capability reuse via test resets. The private
CI mirror compiles that test using AddressSanitizer and UBSan, and links a
real Oreslang guest ARM object into Zephyr QEMU. Do not conflate a host C test
with guest source execution or claim MPU/WCET guarantees.


## Fault cleanup and storage admission (audit 2026-10-14)

A failed generated actor constructor or message-handler turn is retired
at the callback boundary. All private actor field and FIFO storage is
scrubbed; only the last **successfully completed** turn's result and
delivery count remain attached to the originally issued reference. Faults
are sticky even if no actor has yet been spawned; reset cannot clear them.
The adapter does not support resuming after a fault.

The static C adapter is now subject to a **3,072-byte** compile-time
persistent-storage ceiling using `sizeof` of every adapter-owned static
variable. With the audited source compiled for Cortex-M3, the object
contains **2,324 bytes of `.bss`**. Neither number includes Zephyr kernel
RAM, per-thread stacks, link-time alignment, heap elsewhere in the image,
nor WCET. This is not a whole-firmware RAM proof.

The local standalone harness covers 11 fault and lifecycle scenarios on
GCC and Clang under ASan/UBSan (22 test executions) and ARM cross-compilation.
The full source-to-firmware Zephyr/QEMU gate is still required before this
branch can merge.


## Session-wide fatal-fault cleanup

A sticky adapter fault now retires **all** active actor slots whenever the
trusted runtime regains control outside an actor constructor or handler.
The global fail-stop path covers invalid capabilities and telemetry indexes,
mailbox overflow, exhaustion of static actor slots, attempted reentrant sends,
and cross-actor field access. When the fault originates in a guest handler,
cleanup is deferred until the handler has returned, avoiding guest access to
retired actor state. The adapter preserves only telemetry from completed
handler turns, not from the partially executed failing turn.

This is deliberately **session-fatal**: no actor can restart after such a fault
without rebooting the device. The runtime does not implement actor-local
supervision or resumable error recovery in this constrained profile.

Standalone host verification now covers 18 deterministic scenarios on GCC and
Clang with ASan/UBSan (36 executions), plus GCC/Clang static analysis.
A freestanding Cortex-M3 **link test** combines the adapter with a C-defined
stand-in for the generated guest bootstrap and validates an ARM executable
without unresolved symbols. This checks ABI and linkage only; it does not
compile real Oreslang source or execute Zephyr/QEMU. Those gates remain open.


## Non-elidable per-slot sanitization

On successful stop, failed constructor/handler cleanup, or session-wide
fail-stop, the runtime now overwrites the **entire** retired `ActorSlot`
(including padding, queued message values, field state and old callback
pointers) through a `volatile unsigned char` byte loop, then restores only
the slot's monotonically incremented generation. This avoids relying on
ordinary dead stores that optimized C compilers may remove before slot
reuse. Successfully completed scalar delivery telemetry is preserved in
the already-issued reference *before* the slot is scrubbed.

A queued message is similarly byte-scrubbed immediately after dequeue.
The resulting C semantics still require **single-threaded access**:
these volatile writes do not provide thread safety, ISR safety, hardware
MPU isolation, cache/DMA erasure, side-channel resistance, or a
cryptographic guarantee against physical memory recovery.


## Actor-turn telemetry boundary (2026-10-14 audit)

`ores_embedded_actors_last(index)` and
`ores_embedded_actors_deliveries(index)` are **trusted host-only**
postmortem/diagnostic APIs. They must not be callable from any guest actor
constructor or message handler, because an arbitrary index would otherwise
disclose another actor's last returned value and delivery count despite
actor-private field access. Both functions now reject `current != NULL`,
return zero, set a sticky session fault, and retire all live actor slots at
the safe callback boundary. Host access **outside** actor execution still
retrieves completed-turn telemetry, including after a fatal session fault.

The `guest-cross-actor-last`, `guest-cross-actor-deliveries`, and
`guest-constructor-telemetry` native regressions exercise this denial.
This guards the runtime ABI independently of current compiler source-call
admission; it does not make arbitrary C guest callbacks memory-safe.


## Host-only global diagnostic barrier

The `ores_embedded_actors_faulted()`,
`ores_embedded_actors_spawns()`, and `ores_embedded_actors_active()`
entrypoints are also trusted **host-only** diagnostics. A guest
constructor or handler must not infer global fault status or the
existence/count of other actors. Like per-actor `last` and
`deliveries` telemetry, all three functions now return zero and
raise a sticky fatal session fault when called during an actor turn.

The `guest-global-faulted`, `guest-global-spawns`,
`guest-global-active`, and `guest-global-constructor`
regressions prove this boundary at the native runtime ABI.

`scripts/verify-static-actors.sh` runs **27** isolated lifecycle/security
scenarios plus the two-actor C ABI stand-in on each of GCC/Clang at both
`-O1` and `-O2` with AddressSanitizer and UndefinedBehaviorSanitizer:
**112 executions** when run in full. This is a test *requirement*,
not a claim that GitHub-hosted CI has successfully executed this head.


## Cross-actor enqueue and bounded backpressure ABI (2026-10-24)

The static adapter now separates message enqueue from actor execution.
A handler may call `oreslang_source_actor_send(target, value)` only when
`target` is a different live actor. The target handler is never invoked
inline; delivery still occurs later when trusted host/bootstrap code drains
that actor with `stop`. This keeps actor state non-reentrant.

Self-send is deliberately rejected in this constrained profile. Because
`stop` drains until a mailbox is empty, a handler that refilled its own
mailbox could otherwise create an unbounded turn despite the fixed queue
capacity. Constructor sends are rejected as well, so an actor cannot publish
messages before its initialization has completed successfully.

`oreslang_source_actor_try_send` uses the same capability and turn checks,
but returns zero when the target mailbox is full rather than raising the
sticky session fault. Invalid/stale references, self-send, and constructor
use still fail closed. This is a low-level backpressure primitive only:
there is no scheduler yield or `await` continuation in this profile yet.

Native regressions cover successful cross-actor enqueue, full-mailbox
`try_send`, strict full-mailbox fail-stop, self-send rejection and constructor
send rejection. The freestanding ARM ABI stand-in now forwards messages from
a producer callback into a consumer mailbox. It remains a C stand-in, not
proof that current LLVM source lowering can express an ActorRef inside an
Oreslang handler.


## Generation-fenced capability slots (2026-10-24)

Each static actor has at most eight compact runtime-owned capabilities. A
capability stores only the target static slot and target generation. During
`oreslang_source_actor_spawn_with_capabilities`, every supplied ActorRef is
validated as live and its identity is copied; the input pointer array is not
retained. Retirement scrubs the copied capabilities together with the actor's
fields, mailbox and callbacks.

Handlers use `oreslang_source_actor_send_capability(heap, slot, value)` or
`oreslang_source_actor_try_send_capability`. The runtime requires `heap` to
be exactly the currently executing actor, requires handler phase, bounds-checks
the capability index, verifies the target slot is still live at the recorded
generation, and rejects self-targeting. The try form returns zero only for a
full live mailbox; stale or malformed capabilities remain session-fatal.

Native regressions cover copied-pointer independence, full-mailbox try/strict
behavior, stale target generation, constructor use, and out-of-range capability
indices. The ARM ABI stand-in now forwards producer messages through capability
slot zero rather than through a global raw target pointer. This remains ABI
evidence until a real compiler-emitted `.ores` handler targets the slot.
