# Experimental Cortex-M3 static actor runtime

The compiler's source-generated `oreslang_actor_<Type>_init` and
`oreslang_actor_<Type>_on_message` functions run with a statically allocated
C adapter on Zephyr. The admitted `.ores` bootstrap uses `spawn`, `send`
and `stop`; `stop` drains a bounded FIFO for each actor on one cooperative
carrier. All actor state and queue storage is statically allocated.

## Bounded execution contract

- Eight actor slots and at most eight source bootstrap spawns per execution.
- Eight scalar i64 state fields per actor, 16 i64 messages per mailbox.
- Bounded direct `send` calls enqueue without blocking; overflow sets a
  sticky failure flag. A stop drains queued messages and retires its actor.
- Generation-tagged opaque actor references prevent stale sends after a
  slot has been recycled within the same bootstrap.
- All actors execute on a single thread, no guest suspension or concurrent
  handler execution. The only handler input/output is a scalar i64.
- No dynamic allocation exists in the static actor adapter. This does not
  prove the entire Zephyr kernel, toolchain, or firmware never allocates.
- `ores_embedded_actors_reset` is a boot/session initializer, not a
  supported actor lifecycle operation. Caller must discard *all* old handles
  before reset; its fixed ref objects can be reused across sessions.

## Current language boundary

Source `main` can spawn, send literal messages and stop admitted actors.
The generated functions use the existing host actor field ABI, not a new
Oreslang actor syntax. Source actor handlers may update their own private
i64 fields and return an i64. **A handler cannot yet send directly to another
actor**, hold ActorRef fields, suspend/await, or participate in groups. This
first firmware program proves two source-defined actor handler executions
and FIFO delivery, not actor-to-actor application messaging.

## Tests and negative cases

`tests/test_embedded_static_actors.c` covers enqueue/drain, state persistence,
double stop, mailbox capacity and stale pointer after slot reuse. The private
CI mirror compiles that test using AddressSanitizer and UBSan, and links a
real Oreslang guest ARM object into Zephyr QEMU. Do not conflate a host C test
with guest source execution or claim MPU/WCET guarantees.
