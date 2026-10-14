#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "ores_embedded_mailbox.h"

/* Static storage, no dynamic allocation. Reference mailbox is used only from main. */
static ores_embedded_mailbox mailbox;

int main(void) {
    const uint8_t event[] = {1u, 2u};
    uint8_t received[ORES_EMBEDDED_MESSAGE_BYTES];
    size_t n = 0;
    if (ores_embedded_init(&mailbox) != ORES_EMBEDDED_OK ||
        ores_embedded_push(&mailbox, event, sizeof event) != ORES_EMBEDDED_OK ||
        ores_embedded_pop(&mailbox, received, sizeof received, &n) != ORES_EMBEDDED_OK ||
        n != sizeof event || received[0] != 1u) {
        printk("OresEmbedded mailbox reference FAIL\n");
        return 1;
    }
    printk("OresEmbedded mailbox reference PASS\n");
    return 0;
}
