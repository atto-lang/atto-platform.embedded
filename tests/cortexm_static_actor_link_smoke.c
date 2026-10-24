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
static int64_t produce_and_forward(void *heap, uint64_t d, uint64_t e, int64_t v) {
    int64_t next=ores_actor_i64_heap_field_read(heap,d,e,0)+v;
    ores_actor_i64_heap_field_write(heap,d,e,0,next);
    oreslang_source_actor_send_capability(heap,0,v);
    return next;
}
/* Stand-in for LLVM-generated guest ABI; not a real .ores compiler check.
 * The producer callback exercises handler-to-other-actor enqueue without
 * executing the consumer inline. */
static int64_t guest_bootstrap(void) {
    void *b=oreslang_source_actor_spawn(consumer_init,tally,1);
    void *caps[1]={b};
    void *a=oreslang_source_actor_spawn_with_capabilities(
        producer_init,produce_and_forward,1,caps,1);
    caps[0]=0; /* prove the runtime copied capability identity, not the pointer */
    oreslang_source_actor_send(a,1);
    oreslang_source_actor_send(a,2);
    oreslang_source_actor_stop(a);
    oreslang_source_actor_stop(b);
    return 43;
}
int cortexm_static_actor_link_smoke(void) {
    ores_embedded_actors_reset();
    int64_t result=guest_bootstrap();
    return !(result==43 && !ores_embedded_actors_faulted() &&
        ores_embedded_actors_active()==0 && ores_embedded_actors_spawns()==2 &&
        ores_embedded_actors_last(0)==13 && ores_embedded_actors_last(1)==3 &&
        ores_embedded_actors_deliveries(0)==2 &&
        ores_embedded_actors_deliveries(1)==2);
}
