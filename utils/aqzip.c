#include "aqzip.h"

/*#define GET_HASH_DIGEST_LENGTH(hashAlg)                                    \
({                                                                         \
    int length;                                                            \
    if (hashAlg == CPA_CY_SYM_HASH_SHA1)                                   \
    {                                                                      \
        length = 20;                                                       \
    }                                                                      \
    else if (hashAlg == CPA_CY_SYM_HASH_SHA256)                            \
    {                                                                      \
        length = 32;                                                       \
    }                                                                      \
    else                                                                   \
    {                                                                      \
        length = 0;                                                        \
    }                                                                      \
    length;                                                                \
})*/

AQzSession_T g_sess;
QzSessionParams_T g_qz_params_th = {(QzHuffmanHdr_T)0,};
AQzSessionParams_T g_aqz_params_th;
AQzInitParams_T g_init_prarams_th;

const unsigned int g_bufsz_expansion_ratio[] = {5, 20, 50, 100};

FileBuffer_T *g_file_buffers;
char *g_program_name = NULL; /* program name */
int g_decompress = 0;        /* g_decompress (-d) */
int g_digestresult = 1;
int g_keep = 0;                     /* keep (don't delete) input files */
char const g_short_opts[] = "A:H:L:C:r:o:O:dfhkVRDcYS";
const struct option g_long_opts[] = {
    /* { name  has_arg  *flag  val } */
    {"chaining",   0, 0, 'c'},
    {"hash",       0, 0, 'Y'},
    {"sw-execute", 0, 0, 'S'}, /*software compress/decompress/hash */
    {"sample",     0, 0, 'D'}, /* decompress */
    {"decompress", 0, 0, 'd'}, /* decompress */
    {"uncompress", 0, 0, 'd'}, /* decompress */
    {"force",      0, 0, 'f'}, /* force overwrite of output file */
    {"help",       0, 0, 'h'}, /* give help */
    {"keep",       0, 0, 'k'}, /* keep (don't delete) input files */
    {"version",    0, 0, 'V'}, /* display version number */
    {"algorithm",  1, 0, 'A'}, /* set algorithm type */
    {"huffmanhdr", 1, 0, 'H'}, /* set huffman header type */
    {"level",      1, 0, 'L'}, /* set compression level */
    {"chunksz",    1, 0, 'C'}, /* set chunk size */
    {"output",     1, 0, 'O'}, /* set output header format(gzip, gzipext, 7z)*/
    {"recursive",  0, 0, 'R'}, /* set recursive mode when compressing a
                                  directory */
    { 0, 0, 0, 0 }
};

const unsigned int USDM_ALLOC_MAX_SZ = (2 * 1024 * 1024 - 5 * 1024);


static Cpa8U sampleData[] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0xEF, 0xEF, 0xEF, 0x34, 0x53, 0x84, 0x68, 0x76, 0x34, 0x65, 0x36,
    0x45, 0x64, 0xab, 0xd5, 0x27, 0x4a, 0xcb, 0xbb, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xEF, 0xEF, 0xEF,
    0x34, 0x53, 0x84, 0x68, 0x76, 0x34, 0x65, 0x36, 0x45, 0x64, 0xab, 0xd5,
    0x27, 0x4a, 0xcb, 0xbb, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0xEF, 0xEF, 0xEF, 0x34, 0x53, 0x84, 0x68,
    0x76, 0x34, 0x65, 0x36, 0x45, 0x64, 0xab, 0xd5, 0x27, 0x4a, 0xcb, 0xbb,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xDE, 0xAD, 0xEE, 0xEE,
    0xDE, 0xAD, 0xBB, 0xBF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0xEF, 0xEF, 0xEF, 0x34, 0x53, 0x84, 0x68, 0x76, 0x34, 0x65, 0x36,
    0x45, 0x64, 0xab, 0xd5, 0x27, 0x4A, 0xCB, 0xBB
};

int cbFlag = 0;
unsigned int input_sz_thrshold = 0;
unsigned int cb_sz_thrshold = 0;
unsigned int done = 0;
size_t g_src_sz = 0;
size_t g_comp_out_sz = 0;
size_t g_decomp_out_sz = 0;

unsigned int g_src_len = 0;
unsigned int g_dst_len = 0;

char *aqzipBaseName(char *fname)
{
    char *p;

    if ((p = strrchr(fname, '/')) != NULL) {
        fname = p + 1;
    }
    return fname;
}

void help(void)
{
    static char const *const help_msg[] = {
        "Compress or uncompress FILEs (by default, compress FILES in-place).",
        "",
        "Mandatory arguments to long options are mandatory for short options "
        "too.",
        "",
        "  -A, --algorithm   set algorithm type",
        "  -d, --decompress  decompress",
        "  -f, --force       force overwrite of output file and compress links",
        "  -h, --help        give this help",
        "  -H, --huffmanhdr  set huffman header type",
        "  -k, --keep        keep (don't delete) input files",
        "  -V, --version     display version number",
        "  -L, --level       set compression level",
        "  -C, --chunksz     set chunk size",
        "  -c, --chaining    set chaining mode",
        "  -Y, --hash        set hash mode",
        "  -O, --output      set output header format(gzip|gzipext|7z)",
        "  -r,               set max inflight request number",
        "  -R,               set Recursive mode for a directory",
        "  -o,               set output file name",
        "  -S  --sw-execute  set software mode",
        "",
        "With no FILE, read standard input.",
        0
    };
    char const *const *p = help_msg;

    QZ_PRINT("Usage: %s [OPTION]... [FILE]...\n", g_program_name);
    while (*p) {
        QZ_PRINT("%s\n", *p++);
    }
}

/* //Calculate software digest 
static inline int calSWDigest(Cpa8U *msg, Cpa32U slen, Cpa8U *digest, Cpa32U dlen, CpaCySymHashAlgorithm hashAlg)
{
    switch (hashAlg)
    {
        case CPA_CY_SYM_HASH_SHA1:
            return (SHA1(msg, slen, digest) == NULL) ? CPA_STATUS_FAIL
                                                     : CPA_STATUS_SUCCESS;
        case CPA_CY_SYM_HASH_SHA256:
            return (SHA256(msg, slen, digest) == NULL) ? CPA_STATUS_FAIL
                                                       : CPA_STATUS_SUCCESS;
        default:
            // PRINT_ERR("Unsupported hash algorithm %d\n", hashAlg);
            return CPA_STATUS_UNSUPPORTED;
    }
}*/


int aqatzipSetup(AQzSession_T *sess, AQzInitParams_T *init_params, QzSessionParams_T *qz_params, AQzSessionParams_T *aqz_params)
{
    int status;

    QZ_DEBUG("mw>>> sess=%p\n", sess);
    //if (QZ_FUNC_BASIC == sess->func_mode || QZ_FUNC_CHAINING == sess->func_mode) {
    if (QZ_FUNC_BASIC == sess->func_mode) {
        status = aqzInit(sess, init_params, getSwBackup((QzSession_T *)sess));
        if (QZ_OK != status && QZ_DUPLICATE != status && QZ_NO_HW != status) {
            QZ_ERROR("QAT init failed with error: %d\n", status);
            return ERROR;
        }
        QZ_DEBUG("QAT init OK with error: %d\n", status);

	    status = aqzSetupSession(sess, qz_params, aqz_params);
	    if (QZ_OK != status && QZ_DUPLICATE != status && QZ_NO_HW != status) {
	        QZ_ERROR("Session setup failed with error: %d\n", status);
	        return ERROR;
	    }

	    status = aqzInitMem(sess);
	    if (QZ_OK != status) {
	        QZ_ERROR("Mem setup failed with error: %d\n", status);
	        return ERROR;
	    } 
	/*} else if (QZ_FUNC_HASH == sess->func_mode) {
        status = aqzSyInit(sess, init_params, getSwBackup((QzSession_T *)sess));
        if (QZ_OK != status && QZ_DUPLICATE != status && QZ_NO_HW != status) {
            QZ_ERROR("QAT init failed with error: %d\n", status);
            return ERROR;
        }
        printf("QAT init OK with: %d\n", status);

        status = aqzSetupSySession(sess, aqz_params);
        if (QZ_OK != status && QZ_DUPLICATE != status && QZ_NO_HW != status) {
            QZ_ERROR("Session setup failed with error: %d\n", status);
            return ERROR;
        }

        status = aqzInitMem(sess);
	    if (QZ_OK != status) {
	        QZ_ERROR("Mem setup failed with error: %d\n", status);
	        return ERROR;
	    } */
    }

    QZ_DEBUG("Session setup OK with error: %d\n", status);
    return 0;
}

int aqatzipClose(AQzSession_T *sess)
{
    aqzTeardownSession(sess);
    aqzClose(sess);

    return 0;
}

static void callbackCompressFunc(int status, void *res)
{
    AQzQueueHeader_T *header;

    header = (AQzQueueHeader_T *)res;
    cbFlag = 1;
    g_src_sz = header->qz_in_len;
    g_comp_out_sz = header->qz_out_len;
    assert(g_src_sz > g_comp_out_sz);
    return;
}

static void callbackDecompressFunc(int status, void *res)
{
    AQzQueueHeader_T *header;

    header = (AQzQueueHeader_T *)res;
    cbFlag = 1;
    g_decomp_out_sz = header->qz_out_len;
    assert(g_src_sz == g_decomp_out_sz);
    return;
}
/*
int chainCompressAndDecompressSample(AQzSession_T *sess)
{
    // unsigned long crc = 0;
    int res;
    int serial_num = 0;
    g_src_sz = sizeof(sampleData);
    g_comp_out_sz = sizeof(sampleData);
    g_decomp_out_sz = sizeof(sampleData);
    unsigned char *src;
    unsigned char *comp_out;
    unsigned char *decomp_out;
    src = qzMalloc(g_src_sz, 0, PINNED_MEM);
    
    memcpy(src, sampleData, sizeof(sampleData));

    comp_out = qzMalloc(g_comp_out_sz, 0, PINNED_MEM);
    decomp_out = qzMalloc(g_decomp_out_sz, 0, PINNED_MEM);

    sess->func_mode = QZ_FUNC_CHAINING;

    res = aqzChainCompress(sess, src, (uint32_t)(g_src_sz), comp_out,
                           (uint32_t)(g_comp_out_sz), &serial_num, 1, -1, callbackCompressFunc);
    if (0 != res) {
        return -1;
    }

    if (0 == cbFlag) {
        sleep(1);
    }

    cbFlag = 0;

    // res = aqzChainDecompress(sess, comp_out, (uint32_t)(g_comp_out_sz),
    //                           decomp_out, (uint32_t)(g_decomp_out_sz), &serial_num, callbackDecompressFunc);
    // printf("aqzChainDecompress res = %d\n", res);
    if (0 == cbFlag) {
        sleep(1);
    }
    qzFree(src);
    qzFree(comp_out);
    qzFree(decomp_out);
    return res;
}*/

int compressAndDecompressSample(AQzSession_T *sess)
{
    unsigned long crc = 0;
    int serial_num = 0;
    g_src_sz = sizeof(sampleData);
    g_comp_out_sz = sizeof(sampleData);
    g_decomp_out_sz = sizeof(sampleData);
    unsigned char *src;
    unsigned char *comp_out;
    unsigned char *decomp_out;
    src = qzMalloc(g_src_sz, 0, PINNED_MEM);
    
    memcpy(src, sampleData, sizeof(sampleData));

    comp_out = qzMalloc(g_comp_out_sz, 0, PINNED_MEM);
    decomp_out = qzMalloc(g_decomp_out_sz, 0, PINNED_MEM);

    sess->func_mode = QZ_FUNC_BASIC;

    int res = aqzCompressCrc(sess, src, (uint32_t)(g_src_sz), comp_out,
                        (uint32_t)(g_comp_out_sz), &serial_num, &crc, -1, callbackCompressFunc);
    if (QZ_OK != res) {
        goto exit;
    }
    if (0 == cbFlag) {
        sleep(1);
    }

    cbFlag = 0;
    res = aqzDecompress(sess, comp_out, (uint32_t)(g_comp_out_sz),
                        decomp_out, (uint32_t)(g_decomp_out_sz), &serial_num, -1, callbackDecompressFunc);
    if (QZ_OK != res) {
        goto exit;
    }

    if (0 == cbFlag) {
        sleep(1);
    }

exit:
    qzFree(src);
    qzFree(comp_out);
    qzFree(decomp_out);
    return 0;
}

void freeTimeList(RunTimeList_T *time_list)
{
    RunTimeList_T *time_node = time_list;
    RunTimeList_T *pre_time_node = NULL;

    while (time_node) {
        pre_time_node = time_node;
        time_node = time_node->next;
        free(pre_time_node);
    }
}

static void callbackCompressAndDecompressFileFunc(int status, void *res)
{
    if (QZ_FAIL == status) {
        done = 1;
        cbFlag = 1;
        return;
    }
    //int index;
    //int ret;
    AQzQueueHeader_T *header;
    header = (AQzQueueHeader_T *)res;
    // Cpa8U *pSWDigestBuffer = NULL;
    //int digest_nwords = GET_HASH_DIGEST_LENGTH(header->hashAlg)/4;
    //index = *(int *)header->user_info;
    //g_file_buffers[index].dst_len = header->qz_out_len;
    //memcpy((void *)(g_file_buffers[index].dst_buffer), (void *)(header->next_dest), (size_t)(header->qz_out_len)); 
	/*if (QZ_FUNC_CHAINING == header->func_mode || QZ_FUNC_HASH == header->func_mode) {
        ret = calSWDigest(header->src,
                             header->src_sz,
                             g_file_buffers[index].digest_buffer,
                             header->digest_len,
                             header->hashAlg);

        if (CPA_STATUS_SUCCESS == ret)
        {
            if (header->sw_mode == CPA_TRUE) {
                for (int j = 0; j < digest_nwords; j++) {
                    if (to_le32(((uint32_t *) header->next_digest)[j]) !=
                        to_be32(((uint32_t *) g_file_buffers[index].digest_buffer)[j])) {
                        g_digestresult = 0;
                        QZ_ERROR("swhash Digest buffer does not match expected output\n");
                        break;
                    }
                }
            } else {
                if (memcmp(header->next_digest,
                       g_file_buffers[index].digest_buffer,
                       GET_HASH_DIGEST_LENGTH(header->hashAlg)))
                {
                    status = CPA_STATUS_FAIL;
                    g_digestresult = 0;
                    QZ_ERROR("Digest buffer does not match expected output\n");
                }
            }

        }
    }*/


	__sync_add_and_fetch(&cb_sz_thrshold, 1);
    g_src_len = header->qz_in_len;
    g_dst_len = header->qz_out_len;
    cbFlag = 1;
    return;
}

bool hasSuffix(const char *fname)
{
    size_t len = strlen(fname);
    if (len >= strlen(SUFFIX_GZ) &&
        !strcmp(fname + (len - strlen(SUFFIX_GZ)), SUFFIX_GZ)) {
        return 1;
    } else if (len >= strlen(SUFFIX_7Z) &&
               !strcmp(fname + (len - strlen(SUFFIX_7Z)), SUFFIX_7Z)) {
        return 1;
    }
    return 0;
}

int makeOutName(const char *in_name, const char *out_name,
                char *oname, int is_compress)
{
    if (is_compress) {
        if (hasSuffix(in_name)) {
            QZ_ERROR("Warning: %s already has .gz suffix -- unchanged\n",
                     in_name);
            return -1;
        }
        /* add suffix */
        snprintf(oname, MAX_PATH_LEN, "%s%s", out_name ? out_name : in_name,
                 SUFFIX_GZ);
    } else {
        if (!hasSuffix(in_name)) {
            QZ_ERROR("Error: %s: Wrong suffix. Supported suffix: 7z/gz.\n",
                     in_name);
            return -1;
        }
        /* remove suffix */
        snprintf(oname, MAX_PATH_LEN, "%s", out_name ? out_name : in_name);
        if (NULL == out_name) {
            oname[strlen(in_name) - strlen(SUFFIX_GZ)] = '\0';
        }
    }

    return 0;
}

void mkPath(char *path, const char *dirpath, char *file)
{
    if (strlen(dirpath) + strlen(file) + 1 < MAX_PATH_LEN) {
        snprintf(path, MAX_PATH_LEN, "%s/%s", dirpath, file);
    } else {
        assert(0);
    }
}

void processDir(AQzSession_T *sess, const char *in_name,
                const char *out_name, int is_compress)
{
    DIR *dir;
    struct dirent *entry;
    char inpath[MAX_PATH_LEN];

    dir = opendir(in_name);
    assert(dir);

    while ((entry = readdir(dir))) {
        /* Ignore anything starting with ".", which includes the special
         * files ".", "..", as well as hidden files. */
        if (entry->d_name[0] == '.') {
            continue;
        }

        /* Qualify the file with its parent directory to obtain a complete
         * path. */
        mkPath(inpath, in_name, entry->d_name);

        processFile(sess, inpath, out_name, is_compress);
    }
}

void processFile(AQzSession_T *sess, const char *in_name,
                 const char *out_name, int is_compress)
{
    int ret;
    struct stat fstat;

    ret = stat(in_name, &fstat);
    if (ret) {
        perror(in_name);
        exit(-1);
    }

    if (S_ISDIR(fstat.st_mode)) {
        processDir(sess, in_name, out_name, is_compress);
    } else {
        char oname[MAX_PATH_LEN];
        qzMemSet(oname, 0, MAX_PATH_LEN);

        if (makeOutName(in_name, out_name, oname, is_compress)) {
            return;
        }
        doProcessFile(sess, in_name, oname, is_compress);
    }
}

int checkDirectory(const char *filename)
{
    struct stat buf;
    stat(filename, &buf);
    if (S_ISDIR(buf.st_mode)) {
        return 1;
    } else {
        return 0;
    }
}

void displayStats(AQzSession_T* sess, RunTimeList_T *time_list,
                  off_t insize, off_t outsize, int is_compress)
{
    /* Calculate time taken (from begin to end) in micro seconds */
    unsigned long us_begin = 0;
    unsigned long us_end = 0;
    double us_diff = 0;
    RunTimeList_T *time_node = time_list;

    while (time_node) {
        us_begin = time_node->time_s.tv_sec * 1000000 +
                   time_node->time_s.tv_usec;
        us_end = time_node->time_e.tv_sec * 1000000 + time_node->time_e.tv_usec;
        us_diff += (us_end - us_begin);
        time_node = time_node->next;
    }
    /*if (QZ_FUNC_HASH == sess->func_mode || QZ_FUNC_CHAINING == sess->func_mode) {
        if (g_digestresult) {
            QZ_PRINT("Hash Vertify is correct!\n");
        } else {
            QZ_ERROR("Hash Vertify is incorrect!\n");
        }
    }*/

    if (insize) {
        assert(0 != us_diff);
        double size = (is_compress) ? insize : outsize;
        double throughput = (size * CHAR_BIT) / us_diff; /* in MB (megabytes) */
        double compressionRatio = ((double)insize) / ((double)outsize);
        double spaceSavings = 1 - ((double)outsize) / ((double)insize);

        QZ_PRINT("Time taken:    %9.3lf ms\n", us_diff / 1000);
        QZ_PRINT("Throughput:    %9.3lf Mbit/s\n", throughput);
        if (is_compress) {
            QZ_PRINT("Space Savings: %9.3lf %%\n", spaceSavings * 100.0);
            QZ_PRINT("Compression ratio: %.3lf : 1\n", compressionRatio);
        }
    }
}

#define MEM_CHECK(ptr, c)                                            \
    if (NULL == (ptr)) {                                             \
        input_sz_thrshold = c;                                       \
        ret = ERROR;                                                 \
        goto exit;                                                   \
    }

int checkHeader(AQzSession_T *sess, unsigned char *src,
                long src_avail_len, unsigned int *src_buffer_size,
                unsigned int *dst_buffer_size)
{
    QzSess_T * qz_sess;
    unsigned char *src_ptr = src;
    StdGzF_T *qzFooter = NULL;
    QzGzH_T hdr = {{0}, 0};
    QzDataFormat_T data_fmt;

    qz_sess = (QzSess_T *)sess->internal;
    data_fmt = qz_sess->sess_params.data_fmt;

    if (src_avail_len < outputHeaderSz(data_fmt)) {
        QZ_DEBUG("checkHeader: incomplete source buffer\n");
        return QZ_DATA_ERROR;
    }

    if (QZ_DEFLATE_GZIP == data_fmt) {
        qzFooter = (StdGzF_T *)(findStdGzipFooter(src_ptr, src_avail_len));
        hdr.extra.qz_e.dest_sz = (unsigned char *)qzFooter - src_ptr -
                                  stdGzipHeaderSz();
        hdr.extra.qz_e.src_sz = qzFooter->i_size;
    } else if (QZ_OK != qzGzipHeaderExt(src_ptr, &hdr)) {
        return QZ_FAIL;
    }

    *src_buffer_size = (unsigned int)(hdr.extra.qz_e.dest_sz);
    *dst_buffer_size = (unsigned int)(hdr.extra.qz_e.src_sz);

    if (*src_buffer_size + outputHeaderSz(data_fmt) + outputFooterSz(data_fmt) >
        src_avail_len) {
        QZ_DEBUG("checkHeader: incomplete source buffer\n");
        return QZ_DATA_ERROR;
    }

    return QZ_OK;
}

int readForCompress(AQzSession_T *sess, off_t src_file_size, FILE *src_file) {
    int i;
    int ret = OK;
    QzSess_T * qz_sess;
    //AQzSess_T * aqz_sess;
    unsigned int src_buffer_size = 0;
    unsigned int dst_buffer_size = 0;
    //unsigned int digest_buffer_size = 0;
    unsigned int bytes_read = 0;

    input_sz_thrshold = 0;
    qz_sess = (QzSess_T *)sess->internal;
    //aqz_sess = (AQzSess_T *)sess->aqz_sess_params;
    //digest_buffer_size = GET_HASH_DIGEST_LENGTH(aqz_sess->sess_params.cy_hash_algorithm);
    /*if (QZ_FUNC_HASH == sess->func_mode) {
        src_buffer_size = (src_file_size > aqz_sess->sess_params.hw_buff_sz) ?
                    aqz_sess->sess_params.hw_buff_sz : src_file_size;
    } else {
        src_buffer_size = (src_file_size > qz_sess->sess_params.hw_buff_sz) ?
                        qz_sess->sess_params.hw_buff_sz : src_file_size;
    }*/
    src_buffer_size = (src_file_size > qz_sess->sess_params.hw_buff_sz) ?
                    qz_sess->sess_params.hw_buff_sz : src_file_size;
    dst_buffer_size = qzMaxCompressedLength(src_buffer_size, (QzSession_T *)sess);

    input_sz_thrshold = src_file_size / src_buffer_size;
    if (0 != (src_file_size % src_buffer_size)) {
        input_sz_thrshold++;
    }

    g_file_buffers = calloc(input_sz_thrshold, sizeof(FileBuffer_T));
    MEM_CHECK(g_file_buffers, 0);
    for (i = 0; i < input_sz_thrshold; i++) {
        g_file_buffers[i].serial_num = i;

        g_file_buffers[i].src_buffer = malloc(src_buffer_size);
        MEM_CHECK(g_file_buffers[i].src_buffer, i + 1);
        g_file_buffers[i].src_len = src_buffer_size;

        g_file_buffers[i].dst_buffer = malloc(dst_buffer_size);
        MEM_CHECK(g_file_buffers[i].dst_buffer, i + 1);
        g_file_buffers[i].dst_len = dst_buffer_size;
        /*
        g_file_buffers[i].digest_buffer = malloc(digest_buffer_size);
        MEM_CHECK(g_file_buffers[i].digest_buffer, i + 1);
        g_file_buffers[i].digest_len = digest_buffer_size;*/
    }

    for (i = 0; i < input_sz_thrshold; i++) {
        bytes_read = fread(g_file_buffers[i].src_buffer, 1, src_buffer_size, src_file);
        g_file_buffers[i].src_len = bytes_read;
    }
    return ret;

exit:
    for (i = 0; i < input_sz_thrshold; i++) {
        if (NULL != g_file_buffers[i].src_buffer) {
            free(g_file_buffers[i].src_buffer);
            g_file_buffers[i].src_buffer = NULL;
        }
        if (NULL != g_file_buffers[i].dst_buffer) {
            free(g_file_buffers[i].dst_buffer);
            g_file_buffers[i].dst_buffer = NULL;
        }
        if (NULL != g_file_buffers[i].digest_buffer) {
            free(g_file_buffers[i].digest_buffer);
            g_file_buffers[i].digest_buffer = NULL;
        }
    }
    if (NULL != g_file_buffers) {
        free(g_file_buffers);
        g_file_buffers = NULL;
    }
    ret = ERROR;
    return ret;
}

int readForDecompress(AQzSession_T *sess, off_t src_file_size, FILE *src_file) {
    int ret = OK, i;
    QzSess_T * qz_sess;
    off_t file_remaining = 0;
    unsigned char *src_buffer = NULL;
    unsigned char *src_ptr;
    unsigned int src_buffer_size = 0;
    unsigned int dst_buffer_size = 0;
    unsigned int src_buffer_maxsize = 0;
    unsigned int dst_buffer_maxsize = 0;
    unsigned int bytes_read = 0;
    QzDataFormat_T data_fmt;

    qz_sess = (QzSess_T *)sess->internal;
    data_fmt = qz_sess->sess_params.data_fmt;
    input_sz_thrshold = 0;
    src_buffer = malloc(src_file_size);
    if (NULL == src_buffer) {
        goto exit;
    }
    src_ptr = src_buffer;
    bytes_read = fread(src_buffer, 1, src_file_size, src_file);
    file_remaining = bytes_read;

    do {
        ret = checkHeader(sess, src_ptr, file_remaining, &src_buffer_size, &dst_buffer_size);

        if (QZ_OK == ret) {
            input_sz_thrshold++;
            src_ptr += (outputHeaderSz(data_fmt) + src_buffer_size + stdGzipFooterSz());
            file_remaining -= (outputHeaderSz(data_fmt) + src_buffer_size + stdGzipFooterSz());
            if (dst_buffer_maxsize < dst_buffer_size) {
                dst_buffer_maxsize = dst_buffer_size;
            }
            if (src_buffer_maxsize < (outputHeaderSz(data_fmt) + src_buffer_size + stdGzipFooterSz())) {
                src_buffer_maxsize = outputHeaderSz(data_fmt) + src_buffer_size + stdGzipFooterSz();
            }
        } else {
           QZ_ERROR("checkHeader error!\n");
           ret = ERROR;
            return ret;
        }
    } while (file_remaining > 0);

    g_file_buffers = calloc(input_sz_thrshold, sizeof(FileBuffer_T));
    MEM_CHECK(g_file_buffers, 0);
    for (i = 0; i < input_sz_thrshold; i++) {
        g_file_buffers[i].serial_num = i;

        g_file_buffers[i].src_buffer = malloc(src_buffer_maxsize);
        MEM_CHECK(g_file_buffers[i].src_buffer, i + 1);
        g_file_buffers[i].src_len = src_buffer_maxsize;

        g_file_buffers[i].dst_buffer = malloc(dst_buffer_maxsize * 2);
        MEM_CHECK(g_file_buffers[i].dst_buffer, i + 1);
        g_file_buffers[i].dst_len = dst_buffer_maxsize * 2;
    }

    src_ptr = src_buffer;
    file_remaining = bytes_read;
    int index = 0;
    do {
        ret = checkHeader(sess, src_ptr, file_remaining, &src_buffer_size, &dst_buffer_size);

        if (QZ_OK == ret) {
            g_file_buffers[index].src_len = outputHeaderSz(data_fmt) + src_buffer_size + stdGzipFooterSz();
            g_file_buffers[index].dst_len = dst_buffer_size * 2;
            src_ptr += (outputHeaderSz(data_fmt) + src_buffer_size + stdGzipFooterSz());
            file_remaining -= (outputHeaderSz(data_fmt) + src_buffer_size + stdGzipFooterSz());
            index++;
        } else {
            goto exit;
        }
    } while (file_remaining > 0);

    if (NULL != src_buffer) {
        free(src_buffer);
        src_buffer = NULL;
    }
    return ret;

exit:
    if (NULL != src_buffer) {
        free(src_buffer);
        src_buffer = NULL;
    }
    for (i = 0; i < sizeof(g_file_buffers); i++) {
        if (NULL != g_file_buffers[i].src_buffer) {
            free(g_file_buffers[i].src_buffer);
            g_file_buffers[i].src_buffer = NULL;
        }
        if (NULL != g_file_buffers[i].dst_buffer) {
            free(g_file_buffers[i].dst_buffer);
            g_file_buffers[i].dst_buffer = NULL;
        }
    }
    if (NULL != g_file_buffers) {
        free(g_file_buffers);
        g_file_buffers = NULL;
    }
    ret = ERROR;
    return ret;
}

int doReadFile(AQzSession_T *sess, const char *src_file_name, off_t src_file_size, int is_compress)
{
    int ret = OK, i;
    FILE *src_file = NULL;
    unsigned int bytes_read = 0;

    src_file = fopen(src_file_name, "r");
    if (NULL == src_file) {
        ret = ERROR;
        return ret;
    }

    QZ_PRINT("Reading input file %s (%u Bytes)\n", src_file_name, src_file_size);

    if (is_compress ) {
        ret = readForCompress(sess, src_file_size, src_file);
    } else { /* decompress */
        ret = readForDecompress(sess, src_file_size, src_file);
        fclose(src_file);
        src_file = fopen(src_file_name, "r");
        if (NULL == src_file) {
            ret = ERROR;
            return ret;
        }
        for (i = 0; i < input_sz_thrshold; i++) {
            bytes_read = fread(g_file_buffers[i].src_buffer, 1, g_file_buffers[i].src_len, src_file);
            g_file_buffers[i].src_len = bytes_read;
        }
    }

    fclose(src_file);
    return ret;
}

void doProcessFile(AQzSession_T *sess, const char *src_file_name,
                   const char *dst_file_name, int is_compress)
{
    int ret = OK;
    struct stat src_file_stat;
    off_t src_file_size = 0, dst_file_size = 0;
    FILE *dst_file = NULL;
    unsigned int bytes_written = 0;
    int src_fd = 0;
    RunTimeList_T *time_list_head = malloc(sizeof(RunTimeList_T));
    assert(NULL != time_list_head);
    gettimeofday(&time_list_head->time_s, NULL);
    time_list_head->time_e = time_list_head->time_s;
    time_list_head->next = NULL;

    ret = stat(src_file_name, &src_file_stat);
    if (ret) {
        perror(src_file_name);
        exit(ERROR);
    }

    if (S_ISBLK(src_file_stat.st_mode)) {
        if ((src_fd = open(src_file_name, O_RDONLY)) < 0) {
            perror(src_file_name);
            exit(ERROR);
        } else {
            if (ioctl(src_fd, BLKGETSIZE, &src_file_size) < 0) {
                close(src_fd);
                perror(src_file_name);
                exit(ERROR);
            }
            src_file_size *= 512;
            /* size get via BLKGETSIZE is divided by 512 */
            close(src_fd);
        }
    } else {
        src_file_size = src_file_stat.st_size;
    }

    if (0 == src_file_size) {
        QZ_ERROR("Async QATzip can not compress or decompress files of size 0\n");
        goto exit;
    }

    ret = doReadFile(sess, src_file_name, src_file_size, is_compress);
    if (ERROR == ret) {
        QZ_ERROR("Read file error\n");
        goto exit;
    }
    dst_file = fopen(dst_file_name, "w");
    assert(dst_file != NULL);

    int index = 0;
    if (QZ_FUNC_BASIC == sess->func_mode) {
        puts((is_compress) ? "Compressing..." : "Decompressing...");
    /*} else if (QZ_FUNC_CHAINING == sess->func_mode) {
        puts("chainingCompressing...");
    } else if (QZ_FUNC_HASH == sess->func_mode) {
        puts("hashing...");*/
    }

    while (time_list_head->next) {
        time_list_head = time_list_head->next;
    }
    RunTimeList_T *run_time = calloc(1, sizeof(RunTimeList_T));
    assert(NULL != run_time);
    run_time->next = NULL;
    time_list_head->next = run_time;
    time_list_head = run_time;
    gettimeofday(&run_time->time_s, NULL);

    do {
        ret = doProcessBuffer(sess, index, is_compress);

        if (QZ_OK != ret) {
            if (AQZ_NO_SPACE == ret || QZ_NO_INST_ATTACH == ret) {
                continue;
            }
            QZ_ERROR("Process file error: %d\n", ret);
            ret = ERROR;
            goto exit;
        } else {
            index++;
        }
    } while (input_sz_thrshold > index);

    while (input_sz_thrshold != cb_sz_thrshold) {
        usleep(1);
    }
    gettimeofday(&run_time->time_e, NULL);

    for (int i = 0; i < input_sz_thrshold; i++) {
        bytes_written = fwrite(g_file_buffers[i].dst_buffer, 1, g_file_buffers[i].dst_len, dst_file);
        assert(bytes_written == g_file_buffers[i].dst_len);
        dst_file_size += bytes_written;
    }
    input_sz_thrshold = 0;

    displayStats(sess, time_list_head, src_file_size, dst_file_size, is_compress);

    for (int j = 0; j < input_sz_thrshold; j++) {
        if (g_file_buffers[j].src_buffer) {
            free(g_file_buffers[j].src_buffer);
        }
        if (g_file_buffers[j].dst_buffer) {
            free(g_file_buffers[j].dst_buffer);
        }
        if (g_file_buffers[j].digest_buffer) {
            free(g_file_buffers[j].digest_buffer);
        }
    }
exit:
    freeTimeList(time_list_head);
    fclose(dst_file);
    free(g_file_buffers);
    if (!g_keep && OK == ret) {
        unlink(src_file_name);
    }
    if (ret) {
        exit(ret);
    }
}

int doProcessBuffer(AQzSession_T *sess, int index, int is_compress)
{
    int ret = QZ_FAIL;
    unsigned char *src, *dest;
    unsigned int src_len, dest_len;
    void *user_info;
    // QzDataFormat_T data_fmt;
    //unsigned int last;

    src = g_file_buffers[index].src_buffer;
    src_len = g_file_buffers[index].src_len;
    dest = g_file_buffers[index].dst_buffer;
    dest_len = g_file_buffers[index].dst_len;
	// digest = g_file_buffers[index].digest_buffer;
    user_info = (void *)&g_file_buffers[index].serial_num;

    /* Do actual work */
	if (QZ_FUNC_BASIC == sess->func_mode) {
	    if (is_compress) {
            do {
	            ret = aqzCompress(sess, src, src_len, dest, dest_len, user_info, -1, callbackCompressAndDecompressFileFunc);
            } while(AQZ_NO_SPACE == ret || QZ_NO_INST_ATTACH == ret);
        } else {
            do {
	            ret = aqzDecompress(sess, src, src_len, dest, dest_len, user_info, -1, callbackCompressAndDecompressFileFunc);
            } while(AQZ_NO_SPACE == ret || QZ_NO_INST_ATTACH == ret);
        }
    /*} else if (QZ_FUNC_CHAINING == sess->func_mode) {
        if (unlikely(index == input_sz_thrshold)) {
            last = 1;
        } else {
            last = 0;
        }

        if (is_compress) {
            do {
                ret = aqzChainCompress(sess, src, src_len, dest, dest_len, user_info, last, -1, callbackCompressAndDecompressFileFunc);
            } while (AQZ_NO_SPACE == ret || QZ_NO_INST_ATTACH == ret);
        } else {
            QZ_ERROR("chainCompress is unsupported!\n");
            ret = CPA_STATUS_UNSUPPORTED;
        }
    } else if (QZ_FUNC_HASH == sess->func_mode) {
        do {
            ret = aqzHash(sess, src, src_len, dest, dest_len, user_info, -1, callbackCompressAndDecompressFileFunc);
        } while(AQZ_NO_SPACE == ret || QZ_NO_INST_ATTACH == ret);*/
    }

    if (ret != QZ_OK && AQZ_NO_SPACE != ret && QZ_NO_INST_ATTACH != ret) {
        const char *op = (is_compress) ? "Compression" : "Decompression";
        QZ_ERROR("doProcessBuffer2:%s failed with error: %d\n", op, ret);
    }
    return ret;
}
