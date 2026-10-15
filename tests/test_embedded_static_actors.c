#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ores_embedded_static_actors.h"

static void init(void *heap, uint64_t domain, uint64_t epoch) {
    ores_actor_i64_heap_field_init_mutable(heap,domain,epoch,0,10);
}
static int64_t handler(void *heap, uint64_t domain,
                       uint64_t epoch, int64_t msg) {
    const int64_t prior=ores_actor_i64_heap_field_read(heap,domain,epoch,0);
    ores_actor_i64_heap_field_write(heap,domain,epoch,0,prior+msg);
    return prior+msg;
}
static void duplicate_init(void *heap, uint64_t domain, uint64_t epoch) {
    ores_actor_i64_heap_field_init_mutable(heap,domain,epoch,0,10);
    ores_actor_i64_heap_field_init_mutable(heap,domain,epoch,0,11);
}
static int64_t corrupt_handler(void *heap, uint64_t domain,
                               uint64_t epoch, int64_t msg) {
    ores_actor_i64_heap_field_init_mutable(heap,domain,epoch,0,msg);
    return msg;
}
static void const_init(void *heap, uint64_t domain, uint64_t epoch) {
    ores_actor_i64_heap_field_init(heap,domain,epoch,0,3);
}
static int64_t illegal_const_write(void *heap, uint64_t domain,
                                   uint64_t epoch, int64_t msg) {
    ores_actor_i64_heap_field_write(heap,domain,epoch,0,msg);
    return msg;
}
static unsigned fault_after_first_count;
static int64_t fault_after_first(void *heap, uint64_t domain,
                                 uint64_t epoch, int64_t msg) {
    ++fault_after_first_count;
    if (fault_after_first_count > 1)
        ores_actor_i64_heap_field_init_mutable(heap,domain,epoch,0,msg);
    return handler(heap,domain,epoch,msg);
}
static void omitted_init(void *heap, uint64_t domain, uint64_t epoch) {
    (void)heap; (void)domain; (void)epoch;
}

int main(int argc, char **argv) {
    const char *mode=(argc == 2 ? argv[1] : "basic");
    void *first, *second;
    unsigned i;
    ores_embedded_actors_reset();
    if (strcmp(mode,"basic")==0 || strcmp(mode,"stale")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first && !ores_embedded_actors_faulted());
        oreslang_source_actor_send(first,1);
        oreslang_source_actor_send(first,2);
        oreslang_source_actor_stop(first);
        assert(!ores_embedded_actors_faulted());
        assert(ores_embedded_actors_deliveries(0)==2);
        assert(ores_embedded_actors_last(0)==13);
        assert(ores_embedded_actors_active()==0);
        second=oreslang_source_actor_spawn(init,handler,1);
        assert(second && first != second);
        oreslang_source_actor_send(second,5);
        oreslang_source_actor_stop(second);
        assert(!ores_embedded_actors_faulted());
        assert(ores_embedded_actors_last(0)==13);
        assert(ores_embedded_actors_deliveries(0)==2);
        assert(ores_embedded_actors_last(1)==15);
        assert(ores_embedded_actors_deliveries(1)==1);
        if (strcmp(mode,"stale")==0) {
            oreslang_source_actor_send(first,99);
            assert(ores_embedded_actors_faulted());
        }
    } else if (strcmp(mode,"overflow")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first);
        for (i=0; i<ORES_STATIC_MAILBOX_CAPACITY; ++i)
            oreslang_source_actor_send(first,(int64_t)i);
        assert(!ores_embedded_actors_faulted());
        oreslang_source_actor_send(first,999);
        assert(ores_embedded_actors_faulted());
    } else if (strcmp(mode,"reset")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first);
        oreslang_source_actor_stop(first);
        assert(!ores_embedded_actors_faulted());
        ores_embedded_actors_reset(); /* cannot reissue pointer to new actor */
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_spawns()==1);
        assert(oreslang_source_actor_spawn(init,handler,1)==0);
        assert(ores_embedded_actors_spawns()==1);
    } else if (strcmp(mode,"bad-initializer")==0) {
        first=oreslang_source_actor_spawn(duplicate_init,handler,1);
        assert(!first && ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
    } else if (strcmp(mode,"missing-initializer")==0) {
        first=oreslang_source_actor_spawn(omitted_init,handler,1);
        assert(!first && ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
    } else if (strcmp(mode,"bad-handler")==0) {
        first=oreslang_source_actor_spawn(init,corrupt_handler,1);
        assert(first);
        oreslang_source_actor_send(first,1);
        oreslang_source_actor_stop(first);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_deliveries(0)==0);
        assert(ores_embedded_actors_active()==0);
    } else if (strcmp(mode,"fault-after-success")==0) {
        first=oreslang_source_actor_spawn(init,fault_after_first,1);
        assert(first);
        oreslang_source_actor_send(first,1);
        oreslang_source_actor_send(first,2);
        oreslang_source_actor_stop(first);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==1);
        assert(ores_embedded_actors_last(0)==11);
    } else if (strcmp(mode,"immutable-field")==0) {
        first=oreslang_source_actor_spawn(const_init,illegal_const_write,1);
        assert(first);
        oreslang_source_actor_send(first,1);
        oreslang_source_actor_stop(first);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_deliveries(0)==0);
        assert(ores_embedded_actors_active()==0);
    } else if (strcmp(mode,"double-stop")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first);
        oreslang_source_actor_stop(first);
        assert(!ores_embedded_actors_faulted());
        oreslang_source_actor_stop(first);
        assert(ores_embedded_actors_faulted());
    } else {
        return 2;
    }
    return 0;
}
