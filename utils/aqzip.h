#ifndef _UTILS_AQZIP_H
#define _UTILS_AQZIP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include "openssl/sha.h"
#include <sys/time.h>
#include <utime.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <errno.h>
#include <fcntl.h>
#include <qz_utils.h>
#include <qatzip.h> /* new QATzip interface */
#include <cpa_dc.h>
#include <pthread.h>
#include <qatzip_internal.h>
#include <zlib.h>
#include <libgen.h>

#define OK      0
#define ERROR   1

#define SRC_BUFF_LEN         (512 * 1024 * 1024)
#define MAX_PATH_LEN   1024 /* max pathname length */
#define SUFFIX_GZ      ".gz"
#define SUFFIX_7Z      ".7z"

typedef struct RunTimeList_S {
    struct timeval time_s;
    struct timeval time_e;
    struct RunTimeList_S *next;
} RunTimeList_T;

typedef struct FileBuffer_S {
    unsigned int serial_num;
    unsigned char *src_buffer;
    unsigned int src_len;
    unsigned char *dst_buffer;
    unsigned int dst_len;
    unsigned char *digest_buffer;
    unsigned int digest_len;
} FileBuffer_T;

char *aqzipBaseName(char *fname);
void help(void);
void freeTimeList(RunTimeList_T *time_list);
bool hasSuffix(const char *fname);
int makeOutName(const char *in_name, const char *out_name,
                char *oname, int is_compress);
void mkPath(char *path, const char *dirpath, char *file);
int checkDirectory(const char *filename);
void displayStats(AQzSession_T* sess, RunTimeList_T *time_list,
                  off_t insize, off_t outsize, int is_compress);

// int calSWDigest(Cpa8U *msg, Cpa32U slen, Cpa8U *digest, Cpa32U dlen, CpaCySymHashAlgorithm hashAlg);

int aqatzipSetup(AQzSession_T *sess, AQzInitParams_T *init_params, QzSessionParams_T *qz_params, AQzSessionParams_T *aqz_params);
int aqatzipClose(AQzSession_T *sess);

int compressAndDecompressSample(AQzSession_T *sess);
//int chainCompressAndDecompressSample(AQzSession_T *sess);

void processDir(AQzSession_T *sess, const char *in_name,
                const char *out_name, int is_compress);
void processFile(AQzSession_T *sess, const char *in_name,
                 const char *out_name, int is_compress);

void doProcessFile(AQzSession_T *sess, const char *src_file_name,
                   const char *dst_file_name, int is_compress);

int doProcessBuffer(AQzSession_T *sess, int index, int is_compress);

extern char *g_program_name;
extern int g_decompress;        /* g_decompress (-d) */
extern int g_keep;                     /* keep (don't delete) input files */
extern AQzSession_T g_sess;
extern QzSessionParams_T g_qz_params_th;
extern AQzSessionParams_T g_aqz_params_th;
extern AQzInitParams_T g_init_prarams_th;

extern char const g_short_opts[];
extern const struct option g_long_opts[];
extern const unsigned int USDM_ALLOC_MAX_SZ;

#endif