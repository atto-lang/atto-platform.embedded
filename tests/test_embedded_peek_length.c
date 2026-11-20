#include "ores_embedded_mailbox.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

int main(void) {
    ores_embedded_mailbox box;
    uint8_t payload[3] = {7, 8, 9};
    uint8_t out[3] = {0, 0, 0};
    size_t length = 999, written = 999;
    assert(ores_embedded_init(&box) == ORES_EMBEDDED_OK);
    assert(ores_embedded_peek_length(&box, &length) == ORES_EMBEDDED_EMPTY);
    assert(length == 0);
    assert(ores_embedded_peek_length(&box, NULL) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(ores_embedded_push(&box, payload, 3) == ORES_EMBEDDED_OK);
    assert(ores_embedded_peek_length(&box, &length) == ORES_EMBEDDED_OK);
    assert(length == 3 && ores_embedded_count(&box) == 1);
    /* Preflight must neither pop nor scrub the queued message. */
    assert(ores_embedded_pop(&box, out, 2, &written) == ORES_EMBEDDED_BUFFER_TOO_SMALL);
    assert(ores_embedded_peek_length(&box, &length) == ORES_EMBEDDED_OK);
    assert(length == 3);
    assert(ores_embedded_pop(&box, out, 3, &written) == ORES_EMBEDDED_OK);
    assert(written == 3 && out[0] == 7 && out[1] == 8 && out[2] == 9);
    assert(ores_embedded_peek_length(&box, &length) == ORES_EMBEDDED_EMPTY && length == 0);
    assert(ores_embedded_push(&box, NULL, 0) == ORES_EMBEDDED_OK);
    assert(ores_embedded_peek_length(&box, &length) == ORES_EMBEDDED_OK && length == 0);
    /* Never write an output alias within mailbox storage. */
    assert(ores_embedded_peek_length(&box, (size_t *)(void *)&box.slots[box.head].bytes[0])
           == ORES_EMBEDDED_INVALID_ARGUMENT);
    box.slots[box.head].length = ORES_EMBEDDED_MESSAGE_BYTES + 1u;
    length = 123;
    assert(ores_embedded_peek_length(&box, &length) == ORES_EMBEDDED_INVALID_ARGUMENT);
    assert(length == 0);
    return 0;
}
