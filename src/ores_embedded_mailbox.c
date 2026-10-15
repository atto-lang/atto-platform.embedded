#include "ores_embedded_mailbox.h"

/* Deliberately freestanding: no heap allocation, syscalls or libc calls. */
static int valid_state(const ores_embedded_mailbox *box) {
    if (box == NULL) return 0;
    if (box->head >= ORES_EMBEDDED_QUEUE_CAPACITY ||
        box->tail >= ORES_EMBEDDED_QUEUE_CAPACITY ||
        box->count > ORES_EMBEDDED_QUEUE_CAPACITY) return 0;
    return box->tail == (box->head + box->count) % ORES_EMBEDDED_QUEUE_CAPACITY;
}

/* Flat-address embedded targets only; caller must supply valid pointers. */
static int overlaps_box(const ores_embedded_mailbox *box, const void *p, size_t len) {
    uintptr_t a, b;
    if (p == NULL || len == 0) return 0;
    a = (uintptr_t)p;
    b = (uintptr_t)box;
    return (a >= b) ? (a - b < sizeof(*box)) : (b - a < len);
}
/* Non-elidable CPU-side erasure of message bytes, length and padding.
 * Not a DMA/cache/ISR security guarantee; callers require a single thread. */
static void scrub_message(ores_embedded_message *slot) {
    volatile unsigned char *bytes = (volatile unsigned char *)(void *)slot;
    size_t i;
    for (i = 0; i < sizeof(*slot); ++i) bytes[i] = 0;
}
ores_embedded_status ores_embedded_init(ores_embedded_mailbox *box) {
    size_t i;
    if (box == NULL) return ORES_EMBEDDED_INVALID_ARGUMENT;
    volatile unsigned char *dst = (volatile unsigned char *)(void *)box;
    for (i = 0; i < sizeof(*box); ++i)
        dst[i] = 0;
    return ORES_EMBEDDED_OK;
}

ores_embedded_status ores_embedded_push(
    ores_embedded_mailbox *box, const uint8_t *data, size_t length) {
    size_t i;
    ores_embedded_message *slot;
    if (box == NULL || (length != 0 && data == NULL))
        return ORES_EMBEDDED_INVALID_ARGUMENT;
    if (length > ORES_EMBEDDED_MESSAGE_BYTES) return ORES_EMBEDDED_TOO_LARGE;
    if (!valid_state(box) || overlaps_box(box, data, length)) return ORES_EMBEDDED_INVALID_ARGUMENT;
    if (box->count >= ORES_EMBEDDED_QUEUE_CAPACITY) return ORES_EMBEDDED_FULL;
    slot = &box->slots[box->tail];
    for (i = 0; i < length; ++i) slot->bytes[i] = data[i];
    for (; i < ORES_EMBEDDED_MESSAGE_BYTES; ++i) slot->bytes[i] = 0;
    slot->length = (uint16_t)length;
    box->tail = (uint8_t)((box->tail + 1u) % ORES_EMBEDDED_QUEUE_CAPACITY);
    ++box->count;
    return ORES_EMBEDDED_OK;
}

ores_embedded_status ores_embedded_pop(
    ores_embedded_mailbox *box, uint8_t *destination,
    size_t capacity, size_t *written) {
    size_t i;
    ores_embedded_message *slot;
    if (written == NULL) return ORES_EMBEDDED_INVALID_ARGUMENT;
    /* Reject aliases before writing through the caller output pointer. */
    if (box != NULL && overlaps_box(box, written, sizeof(*written)))
        return ORES_EMBEDDED_INVALID_ARGUMENT;
    if (destination != NULL && capacity != 0) {
        uintptr_t a = (uintptr_t)destination;
        uintptr_t b = (uintptr_t)written;
        if ((a >= b && a - b < sizeof(*written)) ||
            (b > a && b - a < capacity))
            return ORES_EMBEDDED_INVALID_ARGUMENT;
    }
    *written = 0;
    if (!valid_state(box) || overlaps_box(box, destination, capacity))
        return ORES_EMBEDDED_INVALID_ARGUMENT;
    if (box->count == 0) return ORES_EMBEDDED_EMPTY;
    slot = &box->slots[box->head];
    if (slot->length > ORES_EMBEDDED_MESSAGE_BYTES) return ORES_EMBEDDED_INVALID_ARGUMENT;
    if (capacity < slot->length) return ORES_EMBEDDED_BUFFER_TOO_SMALL;
    if (slot->length && destination == NULL) return ORES_EMBEDDED_INVALID_ARGUMENT;
    for (i = 0; i < slot->length; ++i) destination[i] = slot->bytes[i];
    *written = slot->length;
    scrub_message(slot);
    box->head = (uint8_t)((box->head + 1u) % ORES_EMBEDDED_QUEUE_CAPACITY);
    --box->count;
    return ORES_EMBEDDED_OK;
}

size_t ores_embedded_count(const ores_embedded_mailbox *box) {
    return valid_state(box) ? box->count : 0u;
}
