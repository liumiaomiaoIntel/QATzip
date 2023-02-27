#include <stdio.h>
//#define __USE_GNU 
#include <sched.h>
#include <pthread.h>
#include <sys/syscall.h>

#include "qatzip.h"
#include "qatzip_internal.h"

//#define CPA_MAX_CORES 256

#define BITMAP_BIT_TEST(bitmask, bit) \
        ((bitmask[(bit)/32]) & (0x1 << ((bit)%32)))

extern processData_T g_process;
extern AProcessData_T g_aqzprocess;
extern __thread AQzIndex_T  g_instance_index;

inline int aqzGetInstance(int hint)
{
    int i, rc;

    if (QZ_NONE == g_process.qz_init_status) {
        return -1;
    }
    i = hint % g_process.num_instances;
    rc = __sync_lock_test_and_set(&(g_process.qz_inst[i].lock), 1);
    g_instance_index.index++;
    if (0 == rc) {
        return i;
    }
    
    return -1;
}


inline int aqz_getQUnusedBuffer(unsigned long i, int j)
{
    int k;
    Cpa32U max;

    max = g_aqzprocess.queue_sz;
    j = g_aqzprocess.aqz_queue[i].queue_idx;
    if (j < 0) {
        j = 0;
    } else if (j >= max) {
        j = 0;
        g_aqzprocess.aqz_queue[i].queue_idx = 0;
    }

    if ((g_aqzprocess.aqz_inst[i].stream[j].src1 == g_aqzprocess.aqz_inst[i].stream[j].src2) &&
        (g_aqzprocess.aqz_inst[i].stream[j].src1 == g_aqzprocess.aqz_inst[i].stream[j].sink1) &&
        (g_aqzprocess.aqz_inst[i].stream[j].src1 == g_aqzprocess.aqz_inst[i].stream[j].sink2)) {
        k = j;
    } else {
        k = -1;
    }
    g_aqzprocess.aqz_queue[i].queue_idx++;
    return k;
}

void grabProcessId()
{
    g_instance_index.pid = getpid();
    g_instance_index.tid = syscall(SYS_gettid);
    return;
}

int codeThreadBind(pthread_t *thread, int i, AQzSession_T *sess)
{
    int rc, j;
    Cpa32U core = -1;
    cpu_set_t cpuset;

    if (QZ_FUNC_BASIC == sess->func_mode || QZ_FUNC_CHAINING == sess->func_mode) {
        if (NULL == thread || i < 0 || i >= g_process.num_instances) {
            return QZ_PARAMS;
        }

        for (j = 0; j < CPA_MAX_CORES; j++) {
            if (BITMAP_BIT_TEST(g_process.qz_inst[i].instance_info.coreAffinity, j)) {
                core = j;
                break;
            }
        }
    } else if (QZ_FUNC_HASH == sess->func_mode) {
        if (NULL == thread || i < 0 || i >= g_aqzprocess.num_sy_instances) {
            return QZ_PARAMS;
        }

        for (j = 0; j < CPA_MAX_CORES; j++) {
            if (BITMAP_BIT_TEST(g_aqzprocess.aqz_syinst[i].instance_info.coreAffinity, j)) {
                core = j;
                break;
            }
        }
    }

    if (-1 == core) {
        return QZ_FAIL;
    }

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    rc = pthread_setaffinity_np(*thread, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        return QZ_FAIL;
    }
    return QZ_OK;
}
