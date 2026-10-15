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
static void *foreign_actor_heap;
static void *reentrant_actor_ref;
static void shared_init(void *heap, uint64_t domain, uint64_t epoch) {
    init(heap,domain,epoch);
    foreign_actor_heap=heap;
}
static int64_t cross_heap_handler(void *heap, uint64_t domain,
                                  uint64_t epoch, int64_t msg) {
    (void)heap;
    (void)ores_actor_i64_heap_field_read(foreign_actor_heap,domain,epoch,0);
    return msg;
}
static int64_t reentrant_send_handler(void *heap, uint64_t domain,
                                      uint64_t epoch, int64_t msg) {
    (void)heap; (void)domain; (void)epoch;
    oreslang_source_actor_send(reentrant_actor_ref,msg+1);
    return msg;
}
/* Privileged native-test view of backing storage, not a guest API. */
static void *marker_heap;
static const int64_t state_marker=INT64_C(0x3141592653589793);
static void marker_init(void *heap, uint64_t domain, uint64_t epoch) {
    marker_heap=heap;
    ores_actor_i64_heap_field_init_mutable(heap,domain,epoch,0,state_marker);
}
static int marker_in_slot_prefix(const void *heap) {
    const unsigned char *data=(const unsigned char*)heap;
    const unsigned char *needle=(const unsigned char*)&state_marker;
    size_t pos;
    /* The slot's scalar field region lies within its first 128 bytes. */
    for(pos=0; pos+sizeof(state_marker)<=128; ++pos) {
        if(memcmp(data+pos,needle,sizeof(state_marker))==0) return 1;
    }
    return 0;
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
    } else if (strcmp(mode,"post-retire-wipe")==0 ||
               strcmp(mode,"fault-retire-wipe")==0) {
        const int fault_mode=strcmp(mode,"fault-retire-wipe")==0;
        first=oreslang_source_actor_spawn(marker_init,handler,1);
        assert(first && marker_heap);
        assert(marker_in_slot_prefix(marker_heap));
        if(fault_mode) {
            for(i=0; i<ORES_STATIC_MAILBOX_CAPACITY; ++i)
                oreslang_source_actor_send(first,(int64_t)i);
            oreslang_source_actor_send(first,INT64_C(789));
            assert(ores_embedded_actors_faulted());
        } else {
            oreslang_source_actor_stop(first);
            assert(!ores_embedded_actors_faulted());
        }
        assert(ores_embedded_actors_active()==0);
        assert(!marker_in_slot_prefix(marker_heap));
        if(!fault_mode) {
            second=oreslang_source_actor_spawn(init,handler,1);
            assert(second && second!=first);
            assert(!marker_in_slot_prefix(marker_heap));
            oreslang_source_actor_stop(second);
            assert(!ores_embedded_actors_faulted());
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
    } else if (strcmp(mode,"fault-before-spawn-reset")==0) {
        oreslang_source_actor_send(0,19); /* no actor has been issued */
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_spawns()==0);
        ores_embedded_actors_reset();
        assert(ores_embedded_actors_faulted());
        assert(oreslang_source_actor_spawn(init,handler,1)==0);
        assert(ores_embedded_actors_spawns()==0);
    } else if (strcmp(mode,"overflow-scrub")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        second=oreslang_source_actor_spawn(init,handler,1);
        assert(first && second);
        oreslang_source_actor_send(second,31);
        for (i=0; i<ORES_STATIC_MAILBOX_CAPACITY; ++i)
            oreslang_source_actor_send(first,(int64_t)i);
        assert(ores_embedded_actors_active()==2);
        oreslang_source_actor_send(first,999);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
        assert(ores_embedded_actors_deliveries(1)==0);
        assert(ores_embedded_actors_last(1)==0);
    } else if (strcmp(mode,"invalid-query-scrub")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        second=oreslang_source_actor_spawn(init,handler,1);
        assert(first && second);
        oreslang_source_actor_send(first,3);
        (void)ores_embedded_actors_last(999);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
    } else if (strcmp(mode,"capacity-stress")==0) {
        void *actors[ORES_STATIC_ACTOR_LIMIT];
        for (i=0; i<ORES_STATIC_ACTOR_LIMIT; ++i) {
            actors[i]=oreslang_source_actor_spawn(init,handler,1);
            assert(actors[i]);
        }
        assert(ores_embedded_actors_active()==ORES_STATIC_ACTOR_LIMIT);
        for (i=0; i<ORES_STATIC_ACTOR_LIMIT; ++i)
            for (unsigned j=0; j<ORES_STATIC_MAILBOX_CAPACITY; ++j)
                oreslang_source_actor_send(actors[i],(int64_t)(i+1));
        assert(!ores_embedded_actors_faulted());
        for (i=ORES_STATIC_ACTOR_LIMIT; i>0; --i)
            oreslang_source_actor_stop(actors[i-1]);
        assert(!ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        for (i=0; i<ORES_STATIC_ACTOR_LIMIT; ++i) {
            assert(ores_embedded_actors_deliveries(i)==ORES_STATIC_MAILBOX_CAPACITY);
            assert(ores_embedded_actors_last(i)==
                10+(int64_t)(i+1)*ORES_STATIC_MAILBOX_CAPACITY);
        }
    } else if (strcmp(mode,"exhaustion-scrub")==0) {
        for (i=0; i<ORES_STATIC_ACTOR_LIMIT; ++i)
            assert(oreslang_source_actor_spawn(init,handler,1));
        assert(ores_embedded_actors_active()==ORES_STATIC_ACTOR_LIMIT);
        assert(!oreslang_source_actor_spawn(init,handler,1));
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_spawns()==ORES_STATIC_ACTOR_LIMIT);
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
    } else if (strcmp(mode,"handler-fault-scrub-others")==0) {
        first=oreslang_source_actor_spawn(init,corrupt_handler,1);
        second=oreslang_source_actor_spawn(init,handler,1);
        assert(first && second);
        oreslang_source_actor_send(first,1);
        oreslang_source_actor_send(second,4);
        oreslang_source_actor_stop(first);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
        assert(ores_embedded_actors_deliveries(1)==0);
    } else if (strcmp(mode,"cross-actor-heap")==0) {
        first=oreslang_source_actor_spawn(init,cross_heap_handler,1);
        second=oreslang_source_actor_spawn(shared_init,handler,1);
        assert(first && second);
        oreslang_source_actor_send(first,1);
        oreslang_source_actor_send(second,2);
        oreslang_source_actor_stop(first);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
        assert(ores_embedded_actors_deliveries(1)==0);
    } else if (strcmp(mode,"reentrant-send")==0) {
        first=oreslang_source_actor_spawn(init,reentrant_send_handler,1);
        assert(first);
        reentrant_actor_ref=first;
        oreslang_source_actor_send(first,1);
        oreslang_source_actor_stop(first);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
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
