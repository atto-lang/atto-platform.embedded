#include "ores_embedded_mailbox.h"

/* Deliberately freestanding: no heap allocation, syscalls or libc calls. */
ores_embedded_status ores_embedded_init(ores_embedded_mailbox *box) {
    size_t i, j;
    if (box == NULL) return ORES_EMBEDDED_INVALID_ARGUMENT;
    for (i = 0; i < ORES_EMBEDDED_QUEUE_CAPACITY; ++i) {
        box->slots[i].length = 0;
        for (j = 0; j < ORES_EMBEDDED_MESSAGE_BYTES; ++j)
            box->slots[i].bytes[j] = 0;
    }
    box->head = 0;
    box->tail = 0;
    box->count = 0;
    return ORES_EMBEDDED_OK;
}

ores_embedded_status ores_embedded_push(
    ores_embedded_mailbox *box, const uint8_t *data, size_t length) {
    size_t i;
    ores_embedded_message *slot;
    if (box == NULL || (length != 0 && data == NULL))
        return ORES_EMBEDDED_INVALID_ARGUMENT;
    if (length > ORES_EMBEDDED_MESSAGE_BYTES) return ORES_EMBEDDED_TOO_LARGE;
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
    *written = 0;
    if (box == NULL) return ORES_EMBEDDED_INVALID_ARGUMENT;
    if (box->count == 0) return ORES_EMBEDDED_EMPTY;
    slot = &box->slots[box->head];
    if (capacity < slot->length) return ORES_EMBEDDED_BUFFER_TOO_SMALL;
    if (slot->length && destination == NULL) return ORES_EMBEDDED_INVALID_ARGUMENT;
    for (i = 0; i < slot->length; ++i) destination[i] = slot->bytes[i];
    *written = slot->length;
    for (i = 0; i < ORES_EMBEDDED_MESSAGE_BYTES; ++i) slot->bytes[i] = 0;
    slot->length = 0;
    box->head = (uint8_t)((box->head + 1u) % ORES_EMBEDDED_QUEUE_CAPACITY);
    --box->count;
    return ORES_EMBEDDED_OK;
}

size_t ores_embedded_count(const ores_embedded_mailbox *box) {
    return box ? box->count : 0u;
}
