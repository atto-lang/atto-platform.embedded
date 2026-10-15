#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "ores_embedded_static_actors.h"

/* This entrypoint is generated from the two-actor .ores source, not C. */
extern int64_t oreslang_cortexm_bootstrap(void);
int main(void) {
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
