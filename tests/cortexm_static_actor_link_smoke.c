#include "ores_embedded_static_actors.h"

static void producer_init(void *heap, uint64_t d, uint64_t e) {
    ores_actor_i64_heap_field_init_mutable(heap,d,e,0,0);
}
static void consumer_init(void *heap, uint64_t d, uint64_t e) {
    ores_actor_i64_heap_field_init_mutable(heap,d,e,0,10);
}
static int64_t tally(void *heap, uint64_t d, uint64_t e, int64_t v) {
    int64_t next=ores_actor_i64_heap_field_read(heap,d,e,0)+v;
    ores_actor_i64_heap_field_write(heap,d,e,0,next);
    return next;
}
/* Stand-in for LLVM-generated guest ABI; not a real .ores compiler check. */
static int64_t guest_bootstrap(void) {
    void *a=oreslang_source_actor_spawn(producer_init,tally,1);
    void *b=oreslang_source_actor_spawn(consumer_init,tally,1);
    oreslang_source_actor_send(a,1);
    oreslang_source_actor_send(b,20);
    oreslang_source_actor_send(a,2);
    oreslang_source_actor_send(b,30);
    oreslang_source_actor_stop(a);
    oreslang_source_actor_stop(b);
    return 43;
}
int cortexm_static_actor_link_smoke(void) {
    ores_embedded_actors_reset();
    int64_t result=guest_bootstrap();
    return !(result==43 && !ores_embedded_actors_faulted() &&
        ores_embedded_actors_active()==0 && ores_embedded_actors_spawns()==2 &&
        ores_embedded_actors_last(0)==3 && ores_embedded_actors_last(1)==60 &&
        ores_embedded_actors_deliveries(0)==2 &&
        ores_embedded_actors_deliveries(1)==2);
}
