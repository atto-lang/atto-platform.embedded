#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include "ores_embedded_static_actors.h"
static void init(void *heap, uint64_t domain, uint64_t epoch) {
    ores_actor_i64_heap_field_init_mutable(heap,domain,epoch,0,10);
}
static int64_t handler(void *heap, uint64_t domain,
                       uint64_t epoch, int64_t msg) {
    int64_t prior=ores_actor_i64_heap_field_read(heap,domain,epoch,0);
    ores_actor_i64_heap_field_write(heap,domain,epoch,0,prior+msg);
    return prior+msg;
}
int main(void) {
    void* first;
    void* second;
    unsigned i;
    ores_embedded_actors_reset();
    first=oreslang_source_actor_spawn(init,handler,1);
    assert(first && !ores_embedded_actors_faulted());
    oreslang_source_actor_send(first,1);
    oreslang_source_actor_send(first,2);
    oreslang_source_actor_stop(first);
    assert(!ores_embedded_actors_faulted());
    assert(ores_embedded_actors_spawns()==1);
    assert(ores_embedded_actors_active()==0);
    assert(ores_embedded_actors_deliveries(0)==2);
    assert(ores_embedded_actors_last(0)==13);
    second=oreslang_source_actor_spawn(init,handler,1);
    assert(second && second != first);
    oreslang_source_actor_send(first,5); /* stale after slot reuse */
    assert(ores_embedded_actors_faulted());
    ores_embedded_actors_reset();
    for (i=0; i<ORES_STATIC_MAILBOX_CAPACITY; ++i) {
        if (!i) first=oreslang_source_actor_spawn(init,handler,1);
        oreslang_source_actor_send(first,(int64_t)i);
    }
    assert(!ores_embedded_actors_faulted());
    oreslang_source_actor_send(first,999);
    assert(ores_embedded_actors_faulted()); /* no silent overflow */
    ores_embedded_actors_reset();
    first=oreslang_source_actor_spawn(init,handler,1);
    assert(first);
    oreslang_source_actor_stop(first);
    assert(!ores_embedded_actors_faulted());
    oreslang_source_actor_stop(first);
    assert(ores_embedded_actors_faulted()); /* stop twice */
    return 0;
}
