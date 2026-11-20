#include "ores_embedded_mailbox.h"
#include <stdio.h>
int main(void) {
    ores_embedded_mailbox box;
    const uint8_t request[] = {0x01u, 0x02u, 0x03u};
    uint8_t response[ORES_EMBEDDED_MESSAGE_BYTES];
    size_t written = 0;
    if (ores_embedded_init(&box) != ORES_EMBEDDED_OK) return 1;
    if (ores_embedded_push(&box, request, sizeof request) != ORES_EMBEDDED_OK) return 1;
    if (ores_embedded_pop(&box, response, sizeof response, &written) != ORES_EMBEDDED_OK) return 1;
    printf("OresEmbedded mailbox reference: %zu bytes, first=%u\n", written,
           written ? (unsigned)response[0] : 0u);
    return written == sizeof request && response[0] == 1u ? 0 : 1;
}
