#ifndef ORES_EMBEDDED_MAILBOX_H
#define ORES_EMBEDDED_MAILBOX_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-size, single-consumer/single-producer EVENT LOOP reference only.
 * Not concurrency safe, not a full actor runtime, and not a hardware sandbox.
 */
#define ORES_EMBEDDED_QUEUE_CAPACITY 8u
#define ORES_EMBEDDED_MESSAGE_BYTES 64u

typedef enum {
    ORES_EMBEDDED_OK = 0,
    ORES_EMBEDDED_INVALID_ARGUMENT = 1,
    ORES_EMBEDDED_TOO_LARGE = 2,
    ORES_EMBEDDED_FULL = 3,
    ORES_EMBEDDED_EMPTY = 4,
    ORES_EMBEDDED_BUFFER_TOO_SMALL = 5
} ores_embedded_status;

typedef struct {
    uint8_t bytes[ORES_EMBEDDED_MESSAGE_BYTES];
    uint16_t length;
} ores_embedded_message;

typedef struct {
    ores_embedded_message slots[ORES_EMBEDDED_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} ores_embedded_mailbox;

ores_embedded_status ores_embedded_init(ores_embedded_mailbox *box);
ores_embedded_status ores_embedded_push(
    ores_embedded_mailbox *box, const uint8_t *data, size_t length);
ores_embedded_status ores_embedded_pop(
    ores_embedded_mailbox *box, uint8_t *destination, size_t capacity,
    size_t *written);
size_t ores_embedded_count(const ores_embedded_mailbox *box);

#ifdef __cplusplus
}
#endif
#endif
