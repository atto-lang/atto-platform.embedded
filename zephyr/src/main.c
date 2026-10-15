#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "ores_embedded_static_actors.h"
#include "ores_embedded_mailbox.h"

/* This entrypoint is generated from the two-actor .ores source, not C. */
extern int64_t oreslang_cortexm_bootstrap(void);

/* Exercise the real freestanding mailbox C implementation in Cortex-M3 QEMU.
 * The source-actor firmware smoke below is a separate acceptance gate. */
static int mailbox_alias_smoke(void) {
    static ores_embedded_mailbox box;
    union {
        size_t alignment;
        uint8_t payload[ORES_EMBEDDED_MESSAGE_BYTES];
    } shared;
    uint8_t value = 91, out = 0;
    size_t written = 99;
    unsigned i;
    if (ores_embedded_init(&box) != ORES_EMBEDDED_OK ||
        ores_embedded_push(&box, &value, 1) != ORES_EMBEDDED_OK)
        return 0;
    shared.alignment = 123;
    if (ores_embedded_pop(&box, shared.payload, sizeof shared.payload,
                          &shared.alignment) != ORES_EMBEDDED_INVALID_ARGUMENT ||
        shared.alignment != 123 || ores_embedded_count(&box) != 1)
        return 0;
    if (ores_embedded_pop(&box, &out, sizeof out, &written) != ORES_EMBEDDED_OK ||
        written != 1 || out != value || ores_embedded_count(&box) != 0)
        return 0;
    for (i = 0; i < 16; ++i) {
        value = (uint8_t)i;
        if (ores_embedded_push(&box, &value, 1) != ORES_EMBEDDED_OK ||
            ores_embedded_pop(&box, &out, sizeof out, &written) != ORES_EMBEDDED_OK ||
            out != value || written != 1 || ores_embedded_count(&box) != 0)
            return 0;
    }
    return 1;
}

int main(void) {
    if (!mailbox_alias_smoke()) {
        printk("ORESLANG_CORTEXM_MAILBOX_ALIAS_FAIL\n");
        return -1;
    }
    printk("ORESLANG_CORTEXM_MAILBOX_ALIAS_PASS\n");
    ores_embedded_actors_reset();
    const int64_t value = oreslang_cortexm_bootstrap();
    if (value != INT64_C(43) || ores_embedded_actors_faulted() ||
        ores_embedded_actors_spawns() != 2 ||
        ores_embedded_actors_active() != 0 ||
        ores_embedded_actors_deliveries(0) != 2 ||
        ores_embedded_actors_deliveries(1) != 2 ||
        ores_embedded_actors_last(0) != INT64_C(3) ||
        ores_embedded_actors_last(1) != INT64_C(60)) {
        printk("ORESLANG_CORTEXM_ACTORS_FAIL\n");
        return -1;
    }
    printk("ORESLANG_CORTEXM_ACTORS_PASS count=2 delivery=4 allocation=static\n");
    return 0;
}
