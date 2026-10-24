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
static void *handler_send_target;
static int handler_try_result;
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
static int64_t cross_actor_send_handler(void *heap, uint64_t domain,
                                        uint64_t epoch, int64_t msg) {
    (void)heap; (void)domain; (void)epoch;
    oreslang_source_actor_send(handler_send_target,msg+1);
    return msg;
}
static int64_t cross_actor_try_send_handler(void *heap, uint64_t domain,
                                            uint64_t epoch, int64_t msg) {
    (void)heap; (void)domain; (void)epoch;
    handler_try_result=oreslang_source_actor_try_send(handler_send_target,msg+1);
    return handler_try_result ? msg : -msg;
}
static void constructor_send_init(void *heap, uint64_t domain, uint64_t epoch) {
    init(heap,domain,epoch);
    oreslang_source_actor_send(handler_send_target,99);
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
static int64_t guest_observed_telemetry;
static int64_t telemetry_read_handler(void *heap, uint64_t domain,
                                      uint64_t epoch, int64_t msg) {
    (void)heap; (void)domain; (void)epoch;
    guest_observed_telemetry=ores_embedded_actors_last(0);
    return msg;
}
static int64_t delivery_read_handler(void *heap, uint64_t domain,
                                     uint64_t epoch, int64_t msg) {
    (void)heap; (void)domain; (void)epoch;
    guest_observed_telemetry=(int64_t)ores_embedded_actors_deliveries(0);
    return msg;
}
static void telemetry_read_init(void *heap, uint64_t domain,
                                uint64_t epoch) {
    init(heap,domain,epoch);
    guest_observed_telemetry=ores_embedded_actors_last(0);
}
/* In guest callbacks these whole-session diagnostics must be opaque.
 * A call must set sticky fault and return a non-disclosing zero. */
static unsigned global_query_kind;
static unsigned global_query_observed;
static int64_t global_query_handler(void *heap, uint64_t domain,
                                    uint64_t epoch, int64_t msg) {
    (void)heap; (void)domain; (void)epoch;
    if (global_query_kind == 0)
        global_query_observed=(unsigned)ores_embedded_actors_faulted();
    else if (global_query_kind == 1)
        global_query_observed=ores_embedded_actors_spawns();
    else
        global_query_observed=ores_embedded_actors_active();
    return msg;
}
static void global_query_constructor(void *heap, uint64_t domain,
                                     uint64_t epoch) {
    init(heap,domain,epoch);
    global_query_observed=ores_embedded_actors_spawns();
}
/* Generated guest handlers must not re-enter lifecycle operations.
 * Reject and scrub the entire session at the safe callback boundary. */
static unsigned guest_lifecycle_action;
static void *guest_lifecycle_target;
static void *guest_spawn_observed;
static int64_t guest_lifecycle_handler(void *heap, uint64_t domain,
                                       uint64_t epoch, int64_t msg) {
    (void)heap; (void)domain; (void)epoch;
    if (guest_lifecycle_action == 0)
        guest_spawn_observed=oreslang_source_actor_spawn(init,handler,1);
    else if (guest_lifecycle_action == 1)
        oreslang_source_actor_stop(guest_lifecycle_target);
    else
        ores_embedded_actors_reset();
    return msg;
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
    } else if (strcmp(mode,"actor-to-actor-send")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        second=oreslang_source_actor_spawn(init,cross_actor_send_handler,1);
        assert(first && second);
        handler_send_target=first;
        oreslang_source_actor_send(second,5);
        oreslang_source_actor_stop(second);
        assert(!ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==1);
        assert(ores_embedded_actors_deliveries(0)==0);
        assert(ores_embedded_actors_deliveries(1)==1);
        assert(ores_embedded_actors_last(1)==5);
        oreslang_source_actor_stop(first);
        assert(!ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==1);
        assert(ores_embedded_actors_last(0)==16);
    } else if (strcmp(mode,"actor-to-actor-try-full")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first);
        for(i=0; i<ORES_STATIC_MAILBOX_CAPACITY; ++i)
            oreslang_source_actor_send(first,(int64_t)i);
        second=oreslang_source_actor_spawn(init,cross_actor_try_send_handler,1);
        assert(second);
        handler_send_target=first;
        handler_try_result=-1;
        oreslang_source_actor_send(second,7);
        oreslang_source_actor_stop(second);
        assert(!ores_embedded_actors_faulted());
        assert(handler_try_result==0);
        assert(ores_embedded_actors_last(1)==-7);
        assert(ores_embedded_actors_active()==1);
        oreslang_source_actor_stop(first);
        assert(!ores_embedded_actors_faulted());
        assert(ores_embedded_actors_deliveries(0)==ORES_STATIC_MAILBOX_CAPACITY);
        assert(ores_embedded_actors_last(0)==130);
    } else if (strcmp(mode,"actor-to-actor-strict-full")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first);
        for(i=0; i<ORES_STATIC_MAILBOX_CAPACITY; ++i)
            oreslang_source_actor_send(first,(int64_t)i);
        second=oreslang_source_actor_spawn(init,cross_actor_send_handler,1);
        assert(second);
        handler_send_target=first;
        oreslang_source_actor_send(second,7);
        oreslang_source_actor_stop(second);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
        assert(ores_embedded_actors_deliveries(1)==0);
    } else if (strcmp(mode,"self-send-rejected")==0) {
        first=oreslang_source_actor_spawn(init,cross_actor_send_handler,1);
        assert(first);
        handler_send_target=first;
        oreslang_source_actor_send(first,1);
        oreslang_source_actor_stop(first);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
    } else if (strcmp(mode,"constructor-send-rejected")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first);
        handler_send_target=first;
        second=oreslang_source_actor_spawn(constructor_send_init,handler,1);
        assert(!second);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_deliveries(0)==0);
    } else if (strcmp(mode,"guest-cross-actor-last")==0 ||
               strcmp(mode,"guest-cross-actor-deliveries")==0 ||
               strcmp(mode,"guest-constructor-telemetry")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first);
        oreslang_source_actor_send(first,7);
        oreslang_source_actor_stop(first);
        assert(!ores_embedded_actors_faulted());
        assert(ores_embedded_actors_last(0)==17);
        assert(ores_embedded_actors_deliveries(0)==1);
        guest_observed_telemetry=-1;
        if (strcmp(mode,"guest-constructor-telemetry")==0) {
            second=oreslang_source_actor_spawn(telemetry_read_init,handler,1);
            assert(!second);
        } else {
            second=oreslang_source_actor_spawn(init,
                strcmp(mode,"guest-cross-actor-last")==0
                    ? telemetry_read_handler : delivery_read_handler,1);
            assert(second);
            oreslang_source_actor_send(second,4);
            oreslang_source_actor_stop(second);
        }
        assert(guest_observed_telemetry==0);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_last(0)==17);
        assert(ores_embedded_actors_deliveries(0)==1);
        assert(ores_embedded_actors_deliveries(1)==0);
    } else if (strcmp(mode,"guest-global-faulted")==0 ||
               strcmp(mode,"guest-global-spawns")==0 ||
               strcmp(mode,"guest-global-active")==0 ||
               strcmp(mode,"guest-global-constructor")==0) {
        first=oreslang_source_actor_spawn(init,handler,1);
        assert(first);
        oreslang_source_actor_send(first,6);
        oreslang_source_actor_stop(first);
        assert(!ores_embedded_actors_faulted());
        assert(ores_embedded_actors_last(0)==16);
        assert(ores_embedded_actors_spawns()==1);
        global_query_observed=999;
        global_query_kind=strcmp(mode,"guest-global-faulted")==0 ? 0 :
            strcmp(mode,"guest-global-spawns")==0 ? 1 : 2;
        if (strcmp(mode,"guest-global-constructor")==0) {
            second=oreslang_source_actor_spawn(global_query_constructor,
                                                handler,1);
            assert(!second);
        } else {
            second=oreslang_source_actor_spawn(init,global_query_handler,1);
            assert(second);
            oreslang_source_actor_send(second,8);
            oreslang_source_actor_stop(second);
        }
        assert(global_query_observed==0);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_last(0)==16);
        assert(ores_embedded_actors_deliveries(0)==1);
        assert(ores_embedded_actors_deliveries(1)==0);
    } else if (strcmp(mode,"guest-spawn")==0 ||
               strcmp(mode,"guest-stop")==0 ||
               strcmp(mode,"guest-reset")==0) {
        guest_lifecycle_action=strcmp(mode,"guest-spawn")==0 ? 0 :
            strcmp(mode,"guest-stop")==0 ? 1 : 2;
        first=oreslang_source_actor_spawn(init,guest_lifecycle_handler,1);
        assert(first);
        guest_lifecycle_target=first;
        guest_spawn_observed=first;
        oreslang_source_actor_send(first,5);
        oreslang_source_actor_stop(first);
        if (guest_lifecycle_action == 0)
            assert(guest_spawn_observed==0);
        assert(ores_embedded_actors_faulted());
        assert(ores_embedded_actors_active()==0);
        assert(ores_embedded_actors_spawns()==1);
        assert(ores_embedded_actors_deliveries(0)==0);
        assert(ores_embedded_actors_last(0)==0);
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
