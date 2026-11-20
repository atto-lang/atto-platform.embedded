/* Differential, reproducible FIFO contract fuzz against an independent model.
 * This is SINGLE-THREADED C11 logic, not an ISR/concurrency or hardware test.
 * Eight distinct fixed seeds x 16,384 operations per seed. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ores_embedded_mailbox.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint32_t step(uint32_t *state) {
    *state = (*state * UINT32_C(1664525)) + UINT32_C(1013904223);
    return *state;
}
static void erased(const ores_embedded_message *message) {
    const volatile unsigned char *p =
        (const volatile unsigned char *)(const void *)message;
    size_t i;
    for (i=0; i<sizeof(*message); ++i) assert(p[i] == 0);
}
int main(void) {
    unsigned seed;
    unsigned full = 0, oversized = 0, short_read = 0, zero = 0;
    for (seed = 1; seed <= 8; ++seed) {
        ores_embedded_mailbox mailbox, before;
        uint8_t expected[ORES_EMBEDDED_QUEUE_CAPACITY][ORES_EMBEDDED_MESSAGE_BYTES];
        size_t lengths[ORES_EMBEDDED_QUEUE_CAPACITY] = {0};
        uint32_t rng = UINT32_C(0x9e3779b9) ^ (uint32_t)seed;
        size_t head = 0, tail = 0, count = 0;
        unsigned i;
        uint8_t bytes[ORES_EMBEDDED_MESSAGE_BYTES];
        size_t written = SIZE_MAX;
        assert(ores_embedded_init(&mailbox) == ORES_EMBEDDED_OK);
        assert(ores_embedded_pop(&mailbox, bytes, sizeof(bytes), &written) ==
               ORES_EMBEDDED_EMPTY && written == 0);
        for (i=0; i<16384; ++i) {
            unsigned action = (step(&rng) >> 16) % 5u;
            if (action < 3) {
                size_t n = (step(&rng) >> 8) % (ORES_EMBEDDED_MESSAGE_BYTES + 2u);
                size_t j;
                ores_embedded_status want, got;
                for (j=0; j<sizeof bytes; ++j) bytes[j] = (uint8_t)(step(&rng) >> 16);
                want = n > ORES_EMBEDDED_MESSAGE_BYTES ? ORES_EMBEDDED_TOO_LARGE :
                    count == ORES_EMBEDDED_QUEUE_CAPACITY ? ORES_EMBEDDED_FULL :
                    ORES_EMBEDDED_OK;
                memcpy(&before, &mailbox, sizeof before);
                got = ores_embedded_push(&mailbox, n ? bytes : NULL, n);
                assert(got == want);
                if (got == ORES_EMBEDDED_OK) {
                    memcpy(expected[tail], bytes, n);
                    lengths[tail] = n;
                    tail = (tail + 1u) % ORES_EMBEDDED_QUEUE_CAPACITY;
                    ++count;
                    if (n == 0) ++zero;
                } else {
                    assert(memcmp(&mailbox, &before, sizeof before) == 0);
                    if (got == ORES_EMBEDDED_FULL) ++full;
                    if (got == ORES_EMBEDDED_TOO_LARGE) ++oversized;
                }
            } else {
                uint8_t destination[ORES_EMBEDDED_MESSAGE_BYTES];
                size_t capacity = (step(&rng) >> 8) % (ORES_EMBEDDED_MESSAGE_BYTES + 1u);
                size_t j, old_head = mailbox.head;
                ores_embedded_status want, got;
                memset(destination, 0xa5, sizeof destination);
                written = SIZE_MAX;
                want = count == 0 ? ORES_EMBEDDED_EMPTY :
                    capacity < lengths[head] ? ORES_EMBEDDED_BUFFER_TOO_SMALL :
                    ORES_EMBEDDED_OK;
                memcpy(&before, &mailbox, sizeof before);
                got = ores_embedded_pop(&mailbox, destination, capacity, &written);
                assert(got == want);
                assert(written == (got == ORES_EMBEDDED_OK ? lengths[head] : 0u));
                if (got == ORES_EMBEDDED_OK) {
                    assert(memcmp(destination, expected[head], written) == 0);
                    for (j=written; j<sizeof destination; ++j)
                        assert(destination[j] == 0xa5);
                    erased(&mailbox.slots[old_head]);
                    head = (head + 1u) % ORES_EMBEDDED_QUEUE_CAPACITY;
                    --count;
                } else {
                    assert(memcmp(&mailbox, &before, sizeof before) == 0);
                    for (j=0; j<sizeof destination; ++j)
                        assert(destination[j] == 0xa5);
                    if (got == ORES_EMBEDDED_BUFFER_TOO_SMALL) ++short_read;
                }
            }
            assert(ores_embedded_count(&mailbox) == count);
            assert(mailbox.head == head && mailbox.tail == tail);
        }
    }
    /* Ensure this seeded model truly visited its mandatory boundary paths. */
    assert(full > 0 && oversized > 0 && short_read > 0 && zero > 0);
    return 0;
}
