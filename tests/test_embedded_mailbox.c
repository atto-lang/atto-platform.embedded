#include "ores_embedded_mailbox.h"
#ifdef NDEBUG
#undef NDEBUG /* do not elide side-effectful test calls */
#endif
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(ores_embedded_mailbox) < 1024, "mailbox must be statically bounded");

int main(void) {
    ores_embedded_mailbox box;
    uint8_t value, output[ORES_EMBEDDED_MESSAGE_BYTES];
    size_t n = 999;
    unsigned i;
    assert(ores_embedded_init(NULL) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
    assert(ores_embedded_count(&box) == 0);
    assert(ores_embedded_pop(&box, output, sizeof output, &n) == ORES_EMBEDDED_EMPTY && n == 0);
    assert(ores_embedded_push(&box, NULL, 1) == ORES_EMBEDDED_INVALID_ARGUMENT);
    value = 5;
    assert(ores_embedded_push(&box, &value, ORES_EMBEDDED_MESSAGE_BYTES + 1u) == ORES_EMBEDDED_TOO_LARGE);
    for (i = 0; i < ORES_EMBEDDED_QUEUE_CAPACITY; ++i) {
        value = (uint8_t)i;
        assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_OK);
    }
    assert(ores_embedded_count(&box) == ORES_EMBEDDED_QUEUE_CAPACITY);
    assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_FULL);
    assert(ores_embedded_pop(&box, output, 0, &n) == ORES_EMBEDDED_BUFFER_TOO_SMALL);
    assert(n == 0 && ores_embedded_count(&box) == ORES_EMBEDDED_QUEUE_CAPACITY);
    assert(ores_embedded_pop(&box, NULL, 1, &n) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_count(&box) == ORES_EMBEDDED_QUEUE_CAPACITY);
    for (i = 0; i < ORES_EMBEDDED_QUEUE_CAPACITY; ++i) {
        assert(ores_embedded_pop(&box, output, sizeof output, &n) == ORES_EMBEDDED_OK);
        assert(n == 1 && output[0] == (uint8_t)i);
        value = (uint8_t)(i + 50);
        assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_OK);
    }
    for (i = 0; i < ORES_EMBEDDED_QUEUE_CAPACITY; ++i) {
        assert(ores_embedded_pop(&box, output, sizeof output, &n) == ORES_EMBEDDED_OK);
        assert(n == 1 && output[0] == (uint8_t)(i + 50));
    }
    assert(ores_embedded_count(&box) == 0);
    assert(ores_embedded_push(&box, NULL, 0) == ORES_EMBEDDED_OK);
    assert(ores_embedded_pop(&box, NULL, 0, &n) == ORES_EMBEDDED_OK && n == 0);
    assert(ores_embedded_pop(&box, output, sizeof output, NULL) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_count(NULL) == 0);
    assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
    box.tail = ORES_EMBEDDED_QUEUE_CAPACITY;
    assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_pop(&box, output, sizeof output, &n) == ORES_EMBEDDED_INVALID_ARGUMENT);
    box.tail = 0; box.head = ORES_EMBEDDED_QUEUE_CAPACITY;
    assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_INVALID_ARGUMENT);
    box.head = 0; box.count = ORES_EMBEDDED_QUEUE_CAPACITY + 1u;
    assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
    assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_OK);
    box.slots[0].length = ORES_EMBEDDED_MESSAGE_BYTES + 1u;
    assert(ores_embedded_pop(&box, output, sizeof output, &n) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
    assert(ores_embedded_push(&box, box.slots[0].bytes, 1) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_OK);
    assert(ores_embedded_pop(&box, box.slots[0].bytes, ORES_EMBEDDED_MESSAGE_BYTES, &n) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_count(&box) == 1);
    {
        union { size_t alignment; ores_embedded_mailbox box; } aliased;
        unsigned char before[sizeof(aliased.box)];
        assert(ores_embedded_init(&aliased.box) == ORES_EMBEDDED_OK);
        value = 77;
        assert(ores_embedded_push(&aliased.box, &value, 1) == ORES_EMBEDDED_OK);
        memcpy(before, &aliased.box, sizeof before);
        assert(ores_embedded_pop(&aliased.box, output, sizeof output,
                                (size_t *)(void *)&aliased.box) == ORES_EMBEDDED_INVALID_ARGUMENT);
        assert(memcmp(before, &aliased.box, sizeof before) == 0);
        assert(ores_embedded_pop(&aliased.box, output, sizeof output, &n) == ORES_EMBEDDED_OK);
        assert(n == 1 && output[0] == 77);
    }
    /* Two external output regions may be disjoint from mailbox storage
     * but still alias one another. Reject without changing either output. */
    {
        union {
            size_t alignment;
            uint8_t bytes[ORES_EMBEDDED_MESSAGE_BYTES];
        } scratch;
        unsigned char before[sizeof scratch];
        assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
        value = 91;
        assert(ores_embedded_push(&box, &value, 1) == ORES_EMBEDDED_OK);
        memset(&scratch, 0x5a, sizeof scratch);
        memcpy(before, &scratch, sizeof before);
        assert(ores_embedded_pop(&box, scratch.bytes, sizeof scratch.bytes,
                                &scratch.alignment) == ORES_EMBEDDED_INVALID_ARGUMENT);
        assert(memcmp(before, &scratch, sizeof before) == 0);
        assert(ores_embedded_count(&box) == 1);
        /* Adjacent-but-disjoint outputs are legal. */
        assert(ores_embedded_pop(&box, scratch.bytes + sizeof(size_t),
                                sizeof scratch.bytes - sizeof(size_t),
                                &scratch.alignment) == ORES_EMBEDDED_OK);
        assert(scratch.alignment == 1);
        assert(scratch.bytes[sizeof(size_t)] == 91);
        assert(ores_embedded_count(&box) == 0);
    }
    /* Every retired mailbox slot, including its message length and any
     * padding, must be zero after a successful delivery. Alternate full and
     * short payloads across repeated ring wraparound to detect stale bytes. */
    {
        uint8_t full[ORES_EMBEDDED_MESSAGE_BYTES];
        size_t j;
        for (j = 0; j < sizeof full; ++j)
            full[j] = (uint8_t)(j + 1u);
        assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
        for (i = 0; i < ORES_EMBEDDED_QUEUE_CAPACITY * 32u; ++i) {
            const size_t length = (i & 1u) ? 1u : sizeof full;
            const unsigned slot = box.tail;
            const unsigned char volatile *cleared;
            assert(ores_embedded_push(&box, full, length) == ORES_EMBEDDED_OK);
            if (length == 1u) {
                for (j = 1; j < ORES_EMBEDDED_MESSAGE_BYTES; ++j)
                    assert(box.slots[slot].bytes[j] == 0);
            }
            memset(output, 0, sizeof output);
            assert(ores_embedded_pop(&box, output, sizeof output, &n) == ORES_EMBEDDED_OK);
            assert(n == length && memcmp(output, full, n) == 0);
            cleared = (const unsigned char volatile *)(const void *)&box.slots[slot];
            for (j = 0; j < sizeof box.slots[slot]; ++j)
                assert(cleared[j] == 0);
            assert(ores_embedded_count(&box) == 0);
        }
        /* A syntactically bounded but internally inconsistent ring must
         * fail closed, including on read-side operations. */
        assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
        box.tail = 1;
        assert(ores_embedded_count(&box) == 0);
        assert(ores_embedded_push(&box, full, 1) == ORES_EMBEDDED_INVALID_ARGUMENT);
        n = 999;
        assert(ores_embedded_pop(&box, output, sizeof output, &n) ==
               ORES_EMBEDDED_INVALID_ARGUMENT && n == 0);
        assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
    }
    return 0;
}
