#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "ores_embedded_mailbox.h"

/* The symbol is provided by the separately cross-compiled .ores ARM object.
 * Volatile inputs prevent the C compiler from folding calls into constants.
 */
extern int64_t ores_embedded_probe(int64_t a, int64_t b);
static ores_embedded_mailbox mailbox;

int main(void) {
    const uint8_t event[] = {10u, 12u};
    uint8_t received[ORES_EMBEDDED_MESSAGE_BYTES];
    size_t n = 0;
    volatile int64_t high = INT64_C(4294967296);
    volatile int64_t low = INT64_C(42);
    volatile int64_t neg_a = INT64_C(-10);
    volatile int64_t neg_b = INT64_C(-12);

    if (ores_embedded_init(&mailbox) != ORES_EMBEDDED_OK ||
        ores_embedded_push(&mailbox, event, sizeof event) != ORES_EMBEDDED_OK ||
        ores_embedded_pop(&mailbox, received, sizeof received, &n) != ORES_EMBEDDED_OK ||
        n != sizeof event || received[0] != 10u || received[1] != 12u) {
        printk("ORESLANG_CORTEXM_MAILBOX_FAIL\n");
        return -1;
    }
    const int64_t result = ores_embedded_probe((int64_t)received[0], (int64_t)received[1]);
    const int64_t large = ores_embedded_probe(high, low);
    const int64_t negative = ores_embedded_probe(neg_a, neg_b);
    if (result != INT64_C(42) ||
        large != INT64_C(12884901930) || negative != INT64_C(-42)) {
        printk("ORESLANG_CORTEXM_AOT_FAIL\n");
        return -1;
    }
    printk("ORESLANG_CORTEXM_AOT_PASS value=42 signed64=PASS\n");
    return 0;
}
