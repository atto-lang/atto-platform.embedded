#include "ores_embedded_mailbox.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

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
    return 0;
}
