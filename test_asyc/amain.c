/***************************************************************************
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2007-2021 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/
 
 
#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <stdio.h>

#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

/* QAT headers */
#include <cpa.h>
#include <cpa_dc.h>

#include <qatzip.h>
#include <qatzip_internal.h>
#include <qz_utils.h>
#include <qae_mem.h>
#include <sys/wait.h>

#define QZ_FMT_NAME         "QZ"
#define GZIP_FMT_NAME       "GZIP"
#define MAX_FMT_NAME        8
#define MAX_NUMA_NODE       8
#define COMP_LVL_MAXIMUM QZ_DEFLATE_COMP_LVL_MAXIMUM

#define ARRAY_LEN(arr)      (sizeof(arr) / sizeof((arr)[0]))
#define KB                  (1024)
#define MB                  (KB * KB)

#define MAX_HUGEPAGE_FILE  "/sys/module/usdm_drv/parameters/max_huge_pages"
#define GET_LOWER_64BITS(v)  ((v) & 0xFFFFFFFFFFFFFFFF)
#define GET_LOWER_32BITS(v)  ((v) & 0xFFFFFFFF)
#define GET_LOWER_16BITS(v)  ((v) & 0xFFFF)
#define GET_LOWER_8BITS(v)   ((v) & 0xFF)

#define ERROR                -2
#define MEM_CHECK(ptr, c)                                            \
    if (NULL == (ptr)) {                                             \
        input_sz_thrshold = c;                                       \
        ret = ERROR;                                                 \
        goto exit;                                                   \
    }

struct task{
  struct promise_type {
    task get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};

typedef void *(QzThdOps)(void *);

typedef enum {
    UNKNOWN,
    QZ,
    GZIP,
    FMT_NUM
} QzFormatId_T;

typedef struct QzFormat_S {
    char fmt_name[MAX_FMT_NAME];
    QzFormatId_T fmt;
} QzFormat_T;

QzFormat_T g_format_list[] = {
    {QZ_FMT_NAME,   QZ},
    {GZIP_FMT_NAME, GZIP}
};

typedef struct QzBlock_S {
    QzFormatId_T fmt;
    unsigned int size;
    struct QzBlock_S *next;
} QzBlock_T;

typedef enum {
    COMP = 0,
    DECOMP,
    BOTH
} ServiceType_T;


typedef struct CPUCore_S {
    int seq;
    int used;
} CPUCore_T;

typedef struct NUMANode_S {
    int num_cores;
    CPUCore_T *core;
} NUMANode_T;

typedef struct {
    int count;
    long long src_sz;
    long long comp_out_sz;
    long long decomp_out_sz;
    unsigned char *src;
} FileBuffer_T;

typedef struct {
    long thd_id;
    ServiceType_T service;
    int count;
    int verify_data;
    int debug;
    long long src_sz;
    long long comp_out_sz;
    long long decomp_out_sz;
    unsigned char *src;
    unsigned char *comp_out;
    unsigned char *decomp_out;
    int gen_data;
    QzSessionParams_T *params;
    AQzSessionParams_T *aparams;
    QzThdOps *ops;
    QzBlock_T *blks;
    int init_engine_disabled;
    int init_sess_disabled;
    int thread_sleep;
    FileBuffer_T *file_buffers;
} TestArg_T;

typedef struct {
    unsigned int request;
    unsigned int response;
    unsigned int dest_len;
} TestResult_T;

const unsigned int USDM_ALLOC_MAX_SZ = (2 * MB - 5 * KB);
const unsigned int DEFAULT_BUF_SZ    = 256 * KB;
const unsigned int QATZIP_MAX_HW_SZ  = 512 * KB;

AQzSession_T g_asession_th;
//QzSession_T g_session_th[100];
QzSessionParams_T g_params_th;
AQzInitParams_T g_init_params;
AQzSessionParams_T g_aparams_th;
static pthread_mutex_t g_lock_print = PTHREAD_MUTEX_INITIALIZER;
char *g_input_file_name = NULL;

extern void dumpAllCounters();
static int test_thread_safe_flag = 0;
extern processData_T g_process;
extern int errno;

static int test_end_flag = 0;
int cbFlag[100];
FileBuffer_T *g_file_buffers;
long long input_sz_thrshold = 0;
int test_flag = 0;
int thrshold_flag = 0;
size_t comp_out_sz_bak =0;
int thread_count = 1;
int tid_flag = 0;

void callbackFunc(int status, void *res)
{
    TestResult_T *result;
    AQzQueueHeader_T *header;
    header = (AQzQueueHeader_T *)res;
    result = (TestResult_T *)header->user_info;

    if (status != 0) {
        QZ_ERROR("ERROR: callbackFunc Failed: %d responseCount: %d\n", status,result->response);
    }
    __sync_add_and_fetch(&comp_out_sz_bak, header->qz_out_len);
    __sync_add_and_fetch(&result->response, 1);
    __sync_add_and_fetch(&test_flag, 1);
    return;
}

void callbackFunccomp(int status, void *res)
{
    TestResult_T *result;
    AQzQueueHeader_T *header;
    header = (AQzQueueHeader_T *)res;
    result = (TestResult_T *)header->user_info;
    
    if (status != 0) {
        QZ_ERROR("ERROR: callbackFunc Failed: %d responseCount: %d\n", status, result->response);
    }

    result->dest_len = header->qz_out_len;
    __sync_add_and_fetch(&result->response, 1);
    return;
}

QzBlock_T *parseFormatOption(char *buf)
{
    char *str = buf, *sub_str = NULL;
    char const *delim = "/", *sub_delim = ":";
    char *token, *sub_token;
    char *saveptr, *sub_saveptr;

    int i, j;
    unsigned int fmt_idx;
    unsigned int fmt_found = 0;
    QzBlock_T *blk = NULL;
    QzBlock_T *head, *prev, *r;
    unsigned int list_len = sizeof(g_format_list) / sizeof(QzFormat_T);

    head = (QzBlock_T *)malloc(sizeof(QzBlock_T));
    assert(NULL != head);
    head->next = NULL;
    prev = head;

    for (i = 1; ; i++, str = NULL) {
        token = strtok_r(str, delim, &saveptr);
        if (NULL == token) {
            break;
        }
        QZ_DEBUG("String[%d]: %s\n", i, token);

        fmt_found = 0;
        blk = NULL;

        for (j = 1, sub_str = token; ; j++, sub_str = NULL) {
            sub_token = strtok_r(sub_str, sub_delim, &sub_saveptr);
            if (NULL == sub_token) {
                break;
            }
            QZ_DEBUG(" -[%d]-> %s\n", j, sub_token);

            if (fmt_found) {
                blk->size = atoi(sub_token);
                break;
            }

            char *tmp = sub_token;
            while (*tmp) {
                *tmp = GET_LOWER_8BITS(toupper(*tmp));
                tmp++;
            }

            for (fmt_idx = 0; fmt_idx < list_len; fmt_idx++) {
                if (0 == strcmp(sub_token, g_format_list[fmt_idx].fmt_name)) {
                    blk = (QzBlock_T *)malloc(sizeof(QzBlock_T));
                    assert(NULL != blk);

                    blk->fmt = g_format_list[fmt_idx].fmt;
                    blk->next = NULL;
                    prev->next = blk;
                    fmt_found = 1;
                    break;
                }
            }
        }

        if (NULL != blk) {
            prev = blk;
        }
    }

    blk = head->next;
    i = 1;
    while (blk) {
        QZ_PRINT("[INFO] Block%d:  format -%8s, \tsize - %d\n",
                 i++, g_format_list[blk->fmt - 1].fmt_name, blk->size);
        blk = blk->next;
    }

    if (NULL == head->next) {
        r = head->next;
        free(head);
    } else {
        r = head;
    }
    return r;
}

static void genRandomData(uint8_t *data, size_t size)
{
    size_t i, j;
    char c;
    uint8_t *ptr = data;

    while (ptr < (data + size)) {
        j = rand() % 100;
        c = GET_LOWER_8BITS((rand() % 65 + 90));
        for (i = (size_t)0; i < j; i++) {
            *ptr = c;
            ptr++;
            if (ptr >= (data + size)) {
                break;
            }
        }
    }
}

static void dumpInputData(size_t size, uint8_t *data)
{
    int fd;
    ssize_t ulen;
    char temp_file[] = "QATZip_Input_XXXXXX";

    if (0 == size || NULL == data)
        return;

    fd = mkstemp(temp_file);
    if (-1 == fd) {
        QZ_ERROR("Creat dump file Failed\n");
        return;
    }

    ulen = write(fd, data, size);
    if (ulen != (ssize_t) size) {
        QZ_ERROR("Creat dump file Failed\n");
        return;
    }
    close(fd);
}

inline int set_cpu(int i)  
{  
    cpu_set_t mask;  
    CPU_ZERO(&mask);  
  
    CPU_SET(i,&mask);  
  
    //printf("thread %lu, i = %d\n", pthread_self(), i);  
    if(-1 == pthread_setaffinity_np(pthread_self() ,sizeof(mask),&mask))  
    {  
        return -1;  
    }  
    return 0;  
}  

void *qzCompressAndDecompress(void *arg)
{
    TestResult_T result = {0, 0, 0};
    int rc = -1, k;
    const int count = ((TestArg_T *)arg)->count;
    unsigned char *src[input_sz_thrshold], *comp_out[input_sz_thrshold], *decomp_out[input_sz_thrshold];

    size_t src_sz[input_sz_thrshold], comp_out_sz[input_sz_thrshold], decomp_out_sz[input_sz_thrshold];//, comp_out_sz_bak =0;
    struct timeval ts, te;
    unsigned long long ts_m, te_m, el_m;
    long double sec, rate;
    size_t *org_src_sz = NULL;
    size_t *org_comp_out_sz = NULL;
    org_src_sz=(size_t *)malloc(sizeof(size_t)*input_sz_thrshold);
    org_comp_out_sz=(size_t *)malloc(sizeof(size_t)*input_sz_thrshold);
    const long tid = ((TestArg_T *)arg)->thd_id;
    const ServiceType_T service = ((TestArg_T *)arg)->service;
    const int verify_data = ((TestArg_T *)arg)->verify_data;
    const int old_src_sz = ((TestArg_T *)arg)->src_sz;

    const int gen_data = ((TestArg_T *)arg)->gen_data;
    int thread_sleep = ((TestArg_T *)arg)->thread_sleep;

    if(set_cpu(tid))  
    {  
        printf("set cpu erro\n");  
    }  
    for (int i = 0; i < input_sz_thrshold; i++) {
        org_src_sz[i]=((TestArg_T *)arg)->file_buffers[i].src_sz;
        org_comp_out_sz[i]=((TestArg_T *)arg)->file_buffers[i].comp_out_sz;
        
        src_sz[i] = org_src_sz[i];
        comp_out_sz[i] = org_comp_out_sz[i];
        decomp_out_sz[i] = org_src_sz[i];
    }

    QZ_DEBUG("Hello from qzCompressAndDecompress tid=%ld, count=%d, service=%d, "
             "verify_data=%d\n",
             tid, count, service, verify_data);
    if (gen_data) {
        for (int i = 0; i < input_sz_thrshold; i++) {
            src[i] = (unsigned char *)qzMalloc(src_sz[i], 0, PINNED_MEM);
            comp_out[i] = (unsigned char *)qzMalloc(comp_out_sz[i], 0, PINNED_MEM);
            decomp_out[i] = (unsigned char *)qzMalloc(decomp_out_sz[i], 0, PINNED_MEM);
        }
    } else {
        for (int i = 0; i < input_sz_thrshold; i++) {
            src[i] = ((TestArg_T *)arg)->file_buffers[i].src;
            comp_out[i] = (unsigned char *)qzMalloc(comp_out_sz[i], 0, PINNED_MEM);
            decomp_out[i] = (unsigned char *)qzMalloc(decomp_out_sz[i], 0, PINNED_MEM);
        }
    }
    if (!src || !comp_out || !decomp_out) {

        QZ_ERROR("Malloc failed\n");
        goto done;
    }
    el_m = 0;
    if (gen_data) {
        QZ_DEBUG("Gen Data...\n");
        for (int i = 0; i < input_sz_thrshold; i++) {
            genRandomData(src[i], src_sz[i]);
        }
    }

    // Compress the data for testing
    if (DECOMP == service) {
        for (int i = 0; i < input_sz_thrshold; i++) {
            rc = aqzCompressSw(&g_asession_th, src[i], (uint32_t)(src_sz[i]), comp_out[i], (uint32_t *)&comp_out_sz[i]);         
            if (rc != 0) {
                QZ_ERROR("ERROR: Compression FAILED with return value: %d\n", rc);
                goto done;
            }
        }
    }
    
    __sync_add_and_fetch(&thrshold_flag, 1);
    while (thrshold_flag != thread_count)
    {
        usleep(0);
    }
        
    (void)gettimeofday(&ts, NULL);
    for (k = 0; k < count; k++) {
        comp_out_sz_bak = 0;
        for (int i = 0; i < input_sz_thrshold; i++) {
            if (DECOMP != service) {
                //resuming_on_new_thread
                comp_out_sz[i] = org_comp_out_sz[i];
                QZ_DEBUG("thread %ld before Compressed %d bytes into %d\n", tid, src_sz[i],
                         comp_out_sz[i]);    
                 
                do {
                    if(tid_flag == 0) {
                        rc = aqzCompress(&g_asession_th, src[i], (uint32_t)(src_sz[i]), comp_out[i],
                                         (uint32_t)(comp_out_sz[i]), &result, tid, callbackFunc);
                    }
                    else {
                        // param inst use tid%3 can get higher throughput.
                        // When there are six instances in the QAT profile, only three instances are used
                        rc = aqzCompress(&g_asession_th, src[i], (uint32_t)(src_sz[i]), comp_out[i],
                                         (uint32_t)(comp_out_sz[i]), &result, tid%8, callbackFunc);           
                    } 
                    
                     if(rc == 13 || rc == -200)
                    {
                      usleep(0);
                      //do
                      //{
                      //    sched_yield();
                      //} while (0);
                    }
                } while (rc == 13 || rc == -200);
                                
                if (rc != QZ_OK) {
                    QZ_ERROR("ERROR: Compression FAILED with return value: %d\n", rc);
                    dumpInputData(src_sz[i], src[i]);
                    goto done;
                }
            }
            if (COMP != service) {
                QZ_DEBUG("thread %ld before Decompressed %d bytes into %d\n", tid, comp_out_sz[i],
                         decomp_out_sz[i]);
                do {
                    if(tid_flag == 0) {
                        rc = aqzDecompress(&g_asession_th, comp_out[i], (uint32_t )(comp_out_sz[i]),
                                         decomp_out[i], (uint32_t)(decomp_out_sz[i]), &result, tid, callbackFunc);
                    }
                    else {
                        // param inst use tid%3 can get higher throughput.
                        // When there are six instances in the QAT profile, only three instances are used
                        rc = aqzDecompress(&g_asession_th, comp_out[i], (uint32_t )(comp_out_sz[i]),
                                           decomp_out[i], (uint32_t)(decomp_out_sz[i]), &result, tid%8, callbackFunc);              
                    }
                     if(rc == 13 || rc == -200)
                    {
                    usleep(0);
                      //do
                      //{
                      //    sched_yield();
                     //} while (0);
                    }
                } while (rc == 13 || rc == -200);    
                
                if (rc != QZ_OK) {
                    QZ_ERROR("ERROR: Decompression FAILED with return value: %d\n", rc);
                    dumpInputData(src_sz[i], src[i]);
                    goto done;
                }
            }
            result.request++;
        }
    }

    while (result.request != result.response) {
        usleep(0);
    }

    (void)gettimeofday(&te, NULL);

    ts_m = (ts.tv_sec * 1000000) + ts.tv_usec;
    te_m = (te.tv_sec * 1000000) + te.tv_usec;
    el_m += te_m - ts_m;

    sec = (long double)(el_m);
    sec = sec / 1000000.0;
    rate = old_src_sz;
    rate /= 1024;
    rate *= 8;// Kbits
    if (BOTH == service) {
        rate *= 2;
    }
    rate *= count;
    rate /= 1024 * 1024; // gigbits

    rate /= sec;// Gbps

    QZ_PRINT("[INFO] srv=");
    if (COMP == service) {
        QZ_PRINT("COMP");
    } else if (DECOMP == service) {
        QZ_PRINT("DECOMP");
    } else if (BOTH == service) {
        QZ_PRINT("BOTH");
    } else {
        QZ_ERROR("UNKNOWN\n");
        pthread_mutex_unlock(&g_lock_print);
        goto done;
    }
    QZ_PRINT(", tid=%ld, verify=%d, count=%d, us=%lld, "
                "bytes=%d, %Lf Gbps",
                tid, verify_data, count, el_m, old_src_sz, rate);

   if (DECOMP != service) {
    QZ_PRINT(", input_len=%d, comp_len=%d, ratio=%f%%",
             old_src_sz, comp_out_sz_bak,
             ((double)comp_out_sz_bak / (double)old_src_sz) * 100);
    }

    if (COMP != service) {
        QZ_PRINT(", produced=%d, decomp_len=%d",
                 comp_out_sz_bak, decomp_out_sz);
    }

    QZ_PRINT("\n");

    if (test_thread_safe_flag == 1) {
        if (thread_sleep == 0) {
            srand(time(NULL));
            thread_sleep = (rand() % 500 + 1) * 1000;
        }
        usleep(thread_sleep);
    }

done:
    if (gen_data) {
        qzFree(src);
        for (int i = 0; i < input_sz_thrshold; i++) {
            qzFree(comp_out[i]);
            qzFree(decomp_out[i]);
        }
        qzFree(comp_out);
        qzFree(decomp_out);
    }
    cbFlag[tid] = 1;
    return 0;
}
/*
void *qzChainCompress(void *arg)
{
    TestResult_T result = {0, 0, 0};
    int rc = -1, k;
    const int count = ((TestArg_T *)arg)->count;
    unsigned char *src[input_sz_thrshold], *comp_out[input_sz_thrshold], *decomp_out[input_sz_thrshold];

    size_t src_sz[input_sz_thrshold], comp_out_sz[input_sz_thrshold], decomp_out_sz[input_sz_thrshold]; //,comp_out_sz_bak;
    struct timeval ts, te;
    unsigned long long ts_m, te_m, el_m;
    long double sec, rate;
    size_t *org_src_sz = NULL;
    size_t *org_comp_out_sz = NULL;
    org_src_sz=(size_t *)malloc(sizeof(size_t)*input_sz_thrshold);
    org_comp_out_sz=(size_t *)malloc(sizeof(size_t)*input_sz_thrshold);
    const long tid = ((TestArg_T *)arg)->thd_id;
    const ServiceType_T service = ((TestArg_T *)arg)->service;
    const int verify_data = ((TestArg_T *)arg)->verify_data;
    const int old_src_sz = ((TestArg_T *)arg)->src_sz;

    const int gen_data = ((TestArg_T *)arg)->gen_data;
    int thread_sleep = ((TestArg_T *)arg)->thread_sleep;
    if(set_cpu(tid))  
    {  
        printf("set cpu erro\n");  
    }  
    for (int i = 0; i < input_sz_thrshold; i++) {
        org_src_sz[i]=((TestArg_T *)arg)->file_buffers[i].src_sz;
        org_comp_out_sz[i]=((TestArg_T *)arg)->file_buffers[i].comp_out_sz;
        
        src_sz[i] = org_src_sz[i];
        comp_out_sz[i] = org_comp_out_sz[i];
        decomp_out_sz[i] = org_src_sz[i];
    }

    QZ_DEBUG("Hello from qzCompressAndDecompress tid=%ld, count=%d, service=%d, "
             "verify_data=%d\n",
             tid, count, service, verify_data);
    if (gen_data) {
        for (int i = 0; i < input_sz_thrshold; i++) {
            src[i] = (unsigned char *)qzMalloc(src_sz[i], 0, PINNED_MEM);
            comp_out[i] = (unsigned char *)qzMalloc(comp_out_sz[i], 0, PINNED_MEM);
            decomp_out[i] = (unsigned char *)qzMalloc(decomp_out_sz[i], 0, PINNED_MEM);
        }
    } else {
        for (int i = 0; i < input_sz_thrshold; i++) {
            src[i] = ((TestArg_T *)arg)->file_buffers[i].src;
            comp_out[i] = (unsigned char *)qzMalloc(comp_out_sz[i], 0, PINNED_MEM);
            decomp_out[i] = (unsigned char *)qzMalloc(decomp_out_sz[i], 0, PINNED_MEM);
        }
    }
    if (!src || !comp_out || !decomp_out) {

        QZ_ERROR("Malloc failed\n");
        goto done;
    }
    el_m = 0;
    if (gen_data) {
        QZ_DEBUG("Gen Data...\n");
        for (int i = 0; i < input_sz_thrshold; i++) {
            genRandomData(src[i], src_sz[i]);
        }
    }
    
    __sync_add_and_fetch(&thrshold_flag, 1);
    while (thrshold_flag != thread_count)
    {
        usleep(1);
    }
    
    (void)gettimeofday(&ts, NULL);
    for (k = 0; k < count; k++) {

        for (int i = 0; i < input_sz_thrshold; i++) {
            //resuming_on_new_thread
            comp_out_sz[i] = org_comp_out_sz[i];
            QZ_DEBUG("thread %ld before Compressed %d bytes into %d\n", tid, src_sz[i],
                        comp_out_sz[i]);    
                
            do {
                  if(tid_flag == 0) {
                      rc = aqzChainCompress(&g_asession_th, src[i], (uint32_t)(src_sz[i]), comp_out[i],
                                                      (uint32_t)(comp_out_sz[i]), &result, 0, tid, callbackFunccomp);                
                  } else {
                      // param inst use tid%3 can get higher throughput.
                      // When there are six instances in the QAT profile, only three instances are used
                      rc = aqzChainCompress(&g_asession_th, src[i], (uint32_t)(src_sz[i]), comp_out[i],
                                                      (uint32_t)(comp_out_sz[i]), &result, 0, tid%3, callbackFunccomp);
                  }

                 if(rc == 13 || rc == -200)
                {
                  usleep(1);
                  //do
                  //{
                  //    sched_yield();
                  //} while (0);
                }                        
            } while (rc == 13 || rc == -200);
                            
            if (rc != QZ_OK) {
                QZ_ERROR("ERROR: Compression FAILED with return value: %d\n", rc);
                dumpInputData(src_sz[i], src[i]);
                goto done;
            }
            result.request++;
        }
    }

    while (result.request != result.response) {
        usleep(1);
    }

    (void)gettimeofday(&te, NULL);

    ts_m = (ts.tv_sec * 1000000) + ts.tv_usec;
    te_m = (te.tv_sec * 1000000) + te.tv_usec;
    el_m += te_m - ts_m;

    sec = (long double)(el_m);
    sec = sec / 1000000.0;
    rate = old_src_sz;
    rate /= 1024;
    rate *= 8;// Kbits
    if (BOTH == service) {
        rate *= 2;
    }
    rate *= count;
    rate /= 1024 * 1024; // gigbits

    rate /= sec;// Gbps

    QZ_PRINT("[INFO] srv=");
    if (COMP == service) {
        QZ_PRINT("COMP");
    } else if (DECOMP == service) {
        QZ_PRINT("DECOMP");
    } else if (BOTH == service) {
        QZ_PRINT("BOTH");
    } else {
        QZ_ERROR("UNKNOWN\n");
        pthread_mutex_unlock(&g_lock_print);
        goto done;
    }
    QZ_PRINT(", tid=%ld, verify=%d, count=%d, us=%lld, "
                "bytes=%d, %Lf Gbps",
                tid, verify_data, count, el_m, old_src_sz, rate);

    QZ_PRINT("\n");

    if (test_thread_safe_flag == 1) {
        if (thread_sleep == 0) {
            srand(time(NULL));
            thread_sleep = (rand() % 500 + 1) * 1000;
        }
        usleep(thread_sleep);
    }

done:
    if (gen_data) {
        qzFree(src);
        qzFree(comp_out);
        qzFree(decomp_out);
    }
    cbFlag[tid] = 1;
    return 0;
}

void *qzaHashCompress(void *arg)
{
    TestResult_T result = {0, 0, 0};
    int rc = -1, k;
    unsigned char *src, *comp_out, *decomp_out;
    size_t src_sz, comp_out_sz, decomp_out_sz;
    struct timeval ts, te;
    unsigned long long ts_m, te_m, el_m;
    long double sec, rate;
    const int org_src_sz = ((TestArg_T *)arg)->src_sz;
    const int org_comp_out_sz = ((TestArg_T *)arg)->comp_out_sz;
    const long tid = ((TestArg_T *)arg)->thd_id;
    const ServiceType_T service = ((TestArg_T *)arg)->service;
    const int verify_data = ((TestArg_T *)arg)->verify_data;
    const int count = ((TestArg_T *)arg)->count;
    const int gen_data = ((TestArg_T *)arg)->gen_data;
    int thread_sleep = ((TestArg_T *)arg)->thread_sleep;
    src_sz = org_src_sz;
    comp_out_sz = org_comp_out_sz;
    decomp_out_sz = org_src_sz;
    if(set_cpu(tid))  
    {  
        printf("set cpu erro\n");  
    }  
    QZ_DEBUG("Hello from qzCompressAndDecompress tid=%ld, count=%d, service=%d, "
             "verify_data=%d\n",
             tid, count, service, verify_data);
    if (gen_data) {
        src = (unsigned char *)aqzMalloc(src_sz, 0, PINNED_MEM);
        comp_out = (unsigned char *)aqzMalloc(comp_out_sz, 0, PINNED_MEM);
        decomp_out = (unsigned char *)aqzMalloc(decomp_out_sz, 0, PINNED_MEM);
    } else {
        src = ((TestArg_T *)arg)->src;
        comp_out = ((TestArg_T *)arg)->comp_out;
        decomp_out = ((TestArg_T *)arg)->decomp_out;
    }
    if (!src || !comp_out || !decomp_out) {
        QZ_ERROR("Malloc failed\n");
        goto done;
    }
    el_m = 0;
    if (gen_data) {
        QZ_DEBUG("Gen Data...\n");
        genRandomData(src, src_sz);
    }
    
    __sync_add_and_fetch(&thrshold_flag, 1);
    while (thrshold_flag != thread_count)
    {
        usleep(1);
    }
    
    (void)gettimeofday(&ts, NULL);
    for (k = 0; k < count; k++) {
          //resuming_on_new_thread
          comp_out_sz = org_comp_out_sz;
          QZ_DEBUG("thread %ld before Compressed %d bytes into %d\n", tid, src_sz,
                      comp_out_sz[i]);    
              
          do {
              if(tid_flag == 0) {
                  rc = aqzHash(&g_asession_th, src, (uint32_t)(src_sz), comp_out,
                                            (uint32_t)(comp_out_sz), &result, tid, callbackFunccomp);
              }
              else {
                  // param inst use tid%3 can get higher throughput.
                  // When there are six instances in the QAT profile, only three instances are used
                  rc = aqzHash(&g_asession_th, src, (uint32_t)(src_sz), comp_out,
                                            (uint32_t)(comp_out_sz), &result, tid%3, callbackFunccomp);
              }
              if(rc == 13 || rc == -200)
              {
                usleep(1);
                //do
                //{
                //    sched_yield();
                //} while (0);
              }  
          } while (rc == 13 || rc == -200);      
          if (rc != QZ_OK) {
              QZ_ERROR("ERROR: Compression FAILED with return value: %d\n", rc);
              dumpInputData(src_sz, src);                goto done;
          }
          result.request++;
    }

    while (result.request != result.response) {
        usleep(1);
    }

    (void)gettimeofday(&te, NULL);

    ts_m = (ts.tv_sec * 1000000) + ts.tv_usec;
    te_m = (te.tv_sec * 1000000) + te.tv_usec;
    el_m += te_m - ts_m;

    sec = (long double)(el_m);
    sec = sec / 1000000.0;
    rate = org_src_sz;
    rate /= 1024;
    rate *= 8;// Kbits
    if (BOTH == service) {
        rate *= 2;
    }
    rate *= count;
    rate /= 1024 * 1024; // gigbits

    rate /= sec;// Gbps

    QZ_PRINT("[INFO] srv=");
    if (COMP == service) {
        QZ_PRINT("COMP");
    } else if (DECOMP == service) {
        QZ_PRINT("DECOMP");
    } else if (BOTH == service) {
        QZ_PRINT("BOTH");
    } else {
        QZ_ERROR("UNKNOWN\n");
        pthread_mutex_unlock(&g_lock_print);
        goto done;
    }
    QZ_PRINT(", tid=%ld, verify=%d, count=%d, us=%lld, "
                "bytes=%d, %Lf Gbps",
                tid, verify_data, count, el_m, org_src_sz, rate);

    QZ_PRINT("\n");

    if (test_thread_safe_flag == 1) {
        if (thread_sleep == 0) {
            srand(time(NULL));
            thread_sleep = (rand() % 500 + 1) * 1000;
        }
        usleep(thread_sleep);
    }

done:
    if (gen_data) {
        qzFree(src);
        qzFree(comp_out);
        qzFree(decomp_out);
    }
    cbFlag[tid] = 1;
    return 0;
}
*/
auto switch_to_new_thread(void *arg) {
    struct awaitable {
        void *p_arg;
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h) {
            void *arg=p_arg;
            std::jthread t([h,arg]
            {
                ((TestArg_T *)arg)->ops(arg);
                h.resume(); 
            });
            t.detach();
        }
        void await_resume() {}
    };
    return awaitable{arg};
}

task resuming_on_new_thread(void *arg) {
    co_await switch_to_new_thread(arg);
}

int readForCompress(unsigned long src_file_size, FILE *src_file) {
    int i;
    int ret = QZ_OK;
    long long src_buffer_size = 0;
    unsigned int bytes_read = 0;

    input_sz_thrshold = 0;
    src_buffer_size = (src_file_size > g_params_th.hw_buff_sz) ?
                        g_params_th.hw_buff_sz : src_file_size;

    input_sz_thrshold = src_file_size / src_buffer_size;

    if (0 != (src_file_size % src_buffer_size)) {
        input_sz_thrshold++;
    }

    g_file_buffers = (FileBuffer_T *)calloc(input_sz_thrshold, sizeof(FileBuffer_T));
    MEM_CHECK(g_file_buffers, 0);
    for (i = 0; i < input_sz_thrshold; i++) {
        g_file_buffers[i].count = i;

        g_file_buffers[i].src = (unsigned char *)qzMalloc(src_buffer_size, 0, COMMON_MEM);
        MEM_CHECK(g_file_buffers[i].src, i + 1);
    }
    for (i = 0; i < input_sz_thrshold; i++) {
        bytes_read = fread(g_file_buffers[i].src, 1, src_buffer_size, src_file);
        g_file_buffers[i].src_sz = bytes_read;
        g_file_buffers[i].comp_out_sz = g_file_buffers[i].src_sz * 2;
        g_file_buffers[i].decomp_out_sz = g_file_buffers[i].src_sz * 5;
    }
    return ret;

exit:
    for (i = 0; i < input_sz_thrshold; i++) {
        if (NULL != g_file_buffers[i].src) {
            qzFree(g_file_buffers[i].src);
        }
    }
    if (NULL != g_file_buffers) {
        free(g_file_buffers);
        g_file_buffers = NULL;
    }
    ret = ERROR;
    return ret;
}

#define STR_INTER(N)    #N
#define STR(N) STR_INTER(N)

#define USAGE_STRING(MAX_LVL)                                                   \
    "Usage: %s [options]\n"                                                     \
    "\n"                                                                        \
    "Required options:\n"                                                       \
    "\n"                                                                        \
    "    -m testMode           1 performance_atests\n"                          \
    "                          2 performance_chaining\n"                        \
    "                          3 performance_hash\n"                            \
    "\n"                                                                        \
    "Optional options can be:\n"                                                \
    "\n"                                                                        \
    "    -i inputfile          input source file\n"                             \
    "                          default by random generate data \n"              \
    "    -t thread_count       maximum forks permitted in the current thread\n" \
    "                          0 means no forking permitted. \n"                \
    "    -l loop count         default is 2\n"                                  \
    "    -v                    verify, disabled by default\n"                   \
    "    -e init engine        enable | disable. enabled by default\n"          \
    "    -s init session       enable | disable. enabled by default\n"          \
    "    -A comp_algorithm     deflate\n"                                       \
    "    -B swBack             0 means disable sw\n"                            \
    "                          1 means enable sw\n"                             \
    "    -C hw_buff_sz         default 64K\n"                                   \
    "    -D direction          comp | decomp | both\n"                          \
    "    -F format             [comp format]:[orig data size]/...\n"            \
    "    -L comp_lvl           1 - " STR(MAX_LVL) "\n"                          \
    "    -O data_fmt           deflate | gzip | gzipext\n"                      \
    "    -T huffmanType        static | dynamic\n"                              \
    "    -r req_cnt_thrshold   max inflight request num, default is 16\n"       \
    "    -S thread_sleep       the unit is milliseconds, default is a random time\n"       \
    "    -Y set hash mode      default 4 SHA256 | 2 SHA1 | 4 SHA256\n"            \
    "    -h                    Print this help message\n"

 void qzPrintUsageAndExit(char *progName)
{
    QZ_ERROR(USAGE_STRING(COMP_LVL_MAXIMUM), progName);
    exit(-1);
}

static int qz_do_g_process_Check(void)
{
    if (g_process.qz_init_status == QZ_OK &&
        g_process.sw_backup == 1 &&
        (g_process.num_instances == 12 || g_process.num_instances == 4 ||
        g_process.num_instances == 32) &&
        g_process.qat_available == CPA_TRUE) {
        return QZ_OK;
    } else {
        return QZ_FAIL;
    }
}

int main(int argc, char *argv[])
{
    int rc = 0, ret = 0, rc_check = 0;
    int i = 0;
    int test = 0;
    ServiceType_T service = COMP;
    TestArg_T test_arg[100];

    unsigned long input_buf_len = QATZIP_MAX_HW_SZ;

    int thread_sleep = 0;

    const char *optstring = "m:t:A:C:D:F:L:T:i:l:e:s:r:B:O:S:Y:Q:vh";
    int opt = 0, loop_cnt = 2, verify = 0;
    int disable_init_engine = 0, disable_init_session = 0;
    char *stop = NULL;
    QzThdOps *qzThdOps = NULL;
    QzBlock_T  *qzBlocks = NULL;
    errno = 0;

    if (aqzGetDefaults(&g_aparams_th) != QZ_OK) {
        return -1;
     }

    if (qzGetDefaults(&g_params_th) != QZ_OK) {
        return -1;
    }

    if (aqzGetInitDefaults(&g_init_params) != QZ_OK) {
        return -1;
    }

    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
        case 'm': // test case
            test = GET_LOWER_32BITS(strtol(optarg, &stop, 0));
            if (*stop != '\0' || errno) {
                QZ_ERROR("Error input: %s\n", optarg);
                return -1;
            }
            break;
        case 't':
            thread_count = GET_LOWER_32BITS(strtol(optarg, &stop, 0));
            g_params_th.max_forks = thread_count;
            if (*stop != '\0' || errno || g_params_th.max_forks > 100) {
                QZ_ERROR("Error thread count arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'A':
            if (strcmp(optarg, "deflate") == 0) {
                g_params_th.comp_algorithm = QZ_DEFLATE;
                } else if (strcmp(optarg, "lz4") == 0) {
                g_params_th.comp_algorithm = QZ_LZ4;
            } else if (strcmp(optarg, "lz4s") == 0) {
                g_params_th.comp_algorithm = QZ_LZ4s;
            } else {
                QZ_ERROR("Error service arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'O':
            if (strcmp(optarg, "deflate") == 0) {
                g_params_th.data_fmt = QZ_DEFLATE_RAW;
            } else if (strcmp(optarg, "gzip") == 0) {
                g_params_th.data_fmt = QZ_DEFLATE_GZIP;
            } else if (strcmp(optarg, "gzipext") == 0) {
                g_params_th.data_fmt = QZ_DEFLATE_GZIP_EXT;
                } else if (strcmp(optarg, "deflate_4B") == 0) {
                g_params_th.data_fmt = QZ_DEFLATE_4B;
            /*
            } else if (strcmp(optarg, "lz4") == 0) {
                g_params_th.data_fmt = LZ4_FH;
            } else if (strcmp(optarg, "lz4s") == 0) {
                    g_params_th.data_fmt = LZ4S_BK;
            */
            } else {
                QZ_ERROR("Error service arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'B':
            g_params_th.sw_backup = GET_LOWER_32BITS(strtol(optarg, &stop, 0));
            if (*stop != '\0' || errno || (g_params_th.sw_backup != 0 &&
                                           g_params_th.sw_backup != 1)) {
                QZ_ERROR("Error input: %s\n", optarg);
                return -1;
            }
            break;
        case 'C':
            g_params_th.hw_buff_sz = GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            g_aparams_th.hw_buff_sz = GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            input_buf_len = GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || errno || g_params_th.hw_buff_sz > USDM_ALLOC_MAX_SZ / 2) {
                QZ_ERROR("Error chunkSize arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'D':
            if (strcmp(optarg, "comp") == 0) {
                service = COMP;
            } else if (strcmp(optarg, "decomp") == 0) {
                service = DECOMP;
            } else if (strcmp(optarg, "both") == 0) {
                service = BOTH;
            } else {
                QZ_ERROR("Error service arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'F':
            qzBlocks = parseFormatOption(optarg);
            if (NULL == qzBlocks) {
                QZ_ERROR("Error format arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'L':
            g_params_th.comp_lvl = GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || errno ||  \
                g_params_th.comp_lvl > COMP_LVL_MAXIMUM || g_params_th.comp_lvl <= 0) {
                QZ_ERROR("Error compLevel arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'T':
            if (strcmp(optarg, "static") == 0) {
                g_params_th.huffman_hdr = QZ_STATIC_HDR;
            } else if (strcmp(optarg, "dynamic") == 0) {
                g_params_th.huffman_hdr = QZ_DYNAMIC_HDR;
            } else {
                QZ_ERROR("Error huffman arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'l':
            loop_cnt = GET_LOWER_32BITS(strtol(optarg, &stop, 0));
            if (*stop != '\0' || errno) {
                QZ_ERROR("Error loop count arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'v':
            verify = 1;
            break;
        case 'i':
            g_input_file_name = optarg;
            break;
        case 'e':
            if (strcmp(optarg, "enable") == 0) {
                disable_init_engine = 0;
            } else if (strcmp(optarg, "disable") == 0) {
                disable_init_engine = 1;
            } else {
                QZ_ERROR("Error init qat engine arg: %s\n", optarg);
                return -1;
            }
            break;
        case 's':
            if (strcmp(optarg, "enable") == 0) {
                disable_init_session = 0;
            } else if (strcmp(optarg, "disable") == 0) {
                disable_init_session = 1;
            } else {
                QZ_ERROR("Error init qat session arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'r':
            g_params_th.req_cnt_thrshold = GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || errno) {
                QZ_ERROR("Error req_cnt_thrshold arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'S':
            thread_sleep = GET_LOWER_32BITS(strtol(optarg, &stop, 0));
            if (*stop != '\0' || errno) {
                QZ_ERROR("Error thread_sleep arg: %s\n", optarg);
                return -1;
            }
            thread_sleep *= 1000;
            break;
            /*
        case 'P':
            if (strcmp(optarg, "busy") == 0) {
                args.polling_mode = QZ_BUSY_POLLING;
            } else {
                QZ_ERROR("Error set polling mode: %s\n", optarg);
                return -1;
            }
            break;
        */
        case 'Y':
            g_aparams_th.cy_hash_algorithm  =  GET_LOWER_32BITS(strtol(optarg, &stop, 0));
            if (*stop != '\0' || errno || (g_aparams_th.cy_hash_algorithm != 2 && g_aparams_th.cy_hash_algorithm != 4)) {
                QZ_ERROR("Error set hash mode: %s\n", optarg);
                return -1;
            }
            break;
        case 'Q':
            tid_flag  =  GET_LOWER_32BITS(strtol(optarg, &stop, 0));
            if (*stop != '\0' || errno ) {
                QZ_ERROR("Error set tid_flag: %s\n", optarg);
                return -1;
            }
            break;
        default:
            qzPrintUsageAndExit(argv[0]);
        }
    }
        
    if (test == 0) {
        qzPrintUsageAndExit(argv[0]);
    }

    if (test == 4 ) {
        if (qzSetDefaults(&g_params_th) != QZ_OK) {
            QZ_ERROR("Err: fail to set default session paramsters.\n");
            return -1;
        }
    }
    /*
    if (test == 5 ) {
        g_params_th.direction= QZ_DIR_COMPRESS;
        g_asession_th.func_mode = QZ_FUNC_CHAINING;
        
        if (qzSetDefaults(&g_params_th) != QZ_OK) {
            QZ_ERROR("Err: fail to set default session paramsters.\n");
            return -1;
        }
        
        if (aqzSetDefaults(&g_aparams_th) != QZ_OK) {
          QZ_ERROR("Err: fail to set default session paramsters.\n");
          return -1;
        }
    }
    if (test == 6) {
        g_asession_th.func_mode = QZ_FUNC_HASH;
        if (aqzSetDefaults(&g_aparams_th) != QZ_OK) {
          QZ_ERROR("Err: fail to set default session paramsters.\n");
          return -1;
        }
    }
    */
    if (g_input_file_name != NULL) {
        FILE *file;
        struct stat file_state;

        if (stat(g_input_file_name, &file_state)) {
            QZ_ERROR("ERROR: fail to get stat of file %s\n", g_input_file_name);
            return -1;
        }
        input_buf_len = GET_LOWER_64BITS(file_state.st_size);

        file = fopen(g_input_file_name, "rb");

        if (!file) {
            QZ_ERROR("ERROR: fail to read file %s\n", g_input_file_name);
            return -1;
        }
        ret = readForCompress( input_buf_len, file);

        fclose(file);
    }

    switch (test) {
    case 4:
        qzThdOps = qzCompressAndDecompress;
        break;
    /*
    case 5:
        qzThdOps = qzChainCompress;
        break;
    case 6:
        qzThdOps = qzaHashCompress;
         break;      
    */         
    default:
        goto done;
    }
        
    for (i = 0; i < thread_count; i++) {
        test_arg[i].thd_id = i;
        test_arg[i].service = service;
        test_arg[i].verify_data = verify;
        test_arg[i].debug = 0;
        test_arg[i].count = loop_cnt;
        test_arg[i].src_sz = GET_LOWER_64BITS(input_buf_len);
        test_arg[i].comp_out_sz = test_arg[i].src_sz * 2;
        test_arg[i].decomp_out_sz = test_arg[i].src_sz * 5;
        test_arg[i].gen_data = g_input_file_name ? 0 : 1;
        test_arg[i].init_engine_disabled = disable_init_engine;
        test_arg[i].init_sess_disabled = disable_init_session;
        if(test == 6){
          test_arg[i].aparams = &g_aparams_th;
        }
        else
        {
         test_arg[i].params = &g_params_th;
        }
        test_arg[i].ops = qzThdOps;
        test_arg[i].blks = qzBlocks;
        test_arg[i].thread_sleep = thread_sleep;
        test_arg[i].file_buffers = g_file_buffers;
        // if (!test_arg[i].comp_out || !test_arg[i].decomp_out) {
        //     QZ_ERROR("ERROR: fail to create memory for thread %ld\n", i);
        //     return -1;
        // }
    }

    srand((uint32_t)getpid());

    if (!((TestArg_T *)test_arg)->init_engine_disabled) {
        if(test == 6){
           rc = aqzSyInit(&g_asession_th, &g_init_params , g_aparams_th.sw_backup);
        }
        else{
            rc = aqzInit(&g_asession_th, &g_init_params , g_params_th.sw_backup);
        }
        if (rc != QZ_OK && rc != QZ_DUPLICATE && rc != QZ_NO_HW) {
            QZ_ERROR("ERROR: aqzInit failed: %d\n", rc);
            goto done;
        }
    }

    QZ_DEBUG("qzInit  rc = %d\n", rc);
    
    if (!((TestArg_T *)test_arg)->init_sess_disabled) {    
        /*if(test == 6){
           rc = aqzSetupSySession(&g_asession_th, ((TestArg_T *)test_arg)->aparams);
        }
        else{
            rc = aqzSetupSession(&g_asession_th, ((TestArg_T *)test_arg)->params, NULL);
        }
        */
        rc = aqzSetupSession(&g_asession_th, ((TestArg_T *)test_arg)->params, NULL);
        if (rc != QZ_OK &&
            rc != QZ_NO_INST_ATTACH &&
            rc != QZ_NONE &&
            rc != QZ_NO_HW) {
            QZ_ERROR("ERROR: aqzSetupSession failed: %d\n", rc);
            goto done;
        }

            rc = aqzInitMem(&g_asession_th);
        if (rc != QZ_OK) {
            QZ_ERROR("ERROR: aqzInitMem failed: %d\n", rc);
            goto done;
        }
    }

    test_end_flag = thread_count * loop_cnt * input_sz_thrshold;
    for (i = 0; i < thread_count; i++) {
        cbFlag[i] = 0;
        resuming_on_new_thread((void *)&test_arg[i]);
    }
    while (true) {
        usleep(100);
        int finish = 1;
        for (i = 0; i < thread_count; i++) {
            finish = finish & cbFlag[i];
        }
        if (1 == finish) {    
            usleep(100);
            break;
        }
    } 
    if (test == 18) {
        rc_check = qz_do_g_process_Check();
        if (QZ_OK == rc_check) {
            QZ_PRINT("Check g_process PASSED\n");
        } else {
            ret = -1;
            QZ_PRINT("Check g_process FAILED\n");
        }
    }

done:
    aqzTeardownSession(&g_asession_th);
    aqzClose(&g_asession_th);
    if (NULL != qzBlocks) {
        QzBlock_T *tmp, *blk = qzBlocks;
        while (blk) {
            tmp = blk;
            blk = blk->next;
            free(tmp);
        }
    }
    for (i = 0; i < input_sz_thrshold; i++) {
        if (NULL != g_file_buffers[i].src) {
            qzFree(g_file_buffers[i].src);
        }
    }
    if (NULL != g_file_buffers) {
        free(g_file_buffers);
        g_file_buffers = NULL;
    }
    // for (i = 0; i < thread_count; i++) {
    //     qzFree(test_arg[i].comp_out);
    //     qzFree(test_arg[i].decomp_out);
    // }
    return (ret != 0) ? ret : rc;
}
