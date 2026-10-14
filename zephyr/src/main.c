#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "ores_embedded_mailbox.h"

/* Real Oreslang source function compiled to AAPCS32 i64-returning symbol. */
extern int64_t ores_embedded_probe(void);
static ores_embedded_mailbox mailbox;

int main(void) {
    const uint8_t event[] = {1u, 2u};
    uint8_t received[ORES_EMBEDDED_MESSAGE_BYTES];
    size_t n = 0;
    if (ores_embedded_init(&mailbox) != ORES_EMBEDDED_OK ||
        ores_embedded_push(&mailbox, event, sizeof event) != ORES_EMBEDDED_OK ||
        ores_embedded_pop(&mailbox, received, sizeof received, &n) != ORES_EMBEDDED_OK ||
        n != sizeof event || received[0] != 1u) {
        printk("ORESLANG_CORTEXM_MAILBOX_FAIL\n");
        return -1;
    }
    if (ores_embedded_probe() != INT64_C(42)) {
        printk("ORESLANG_CORTEXM_AOT_FAIL\n");
        return -1;
    }
    printk("ORESLANG_CORTEXM_AOT_PASS value=42\n");
    return 0;
}
