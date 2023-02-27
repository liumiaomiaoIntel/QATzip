#include <stdlib.h>

#include "qatzip.h"
#include "qatzip_internal.h"
#include "qz_utils.h"
#include <isa-l.h>
//#include <isa-l_crypto.h>
#include "openssl/sha.h"

#define SHA1_DIGEST_WORDS				 5
#define WORD_BYTES                       4

#define GET_HASH_DIGEST_LENGTH(hashAlg)                                    \
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
})


extern AProcessData_T g_aqzprocess;

int aqzISALCompress(AQzQueueBufferList_T *buffer_data, CpaDcRqResults *resl)
{
    int rc;
    AQzQueueHeader_T *header;
    struct isal_zstream stream;
    // struct isal_gzip_header gz_hdr;
    unsigned char *level_buf = NULL;
    unsigned char *next_dest;
    QzDataFormat_T data_fmt;

    header = &buffer_data->header;
    next_dest = header->next_dest;
    data_fmt = header->data_fmt;

    level_buf = malloc(ISAL_DEF_LVL2_DEFAULT);
    if (NULL == level_buf) {
        return QZ_FAIL;
    }

    next_dest += outputHeaderSz(data_fmt);
    header->qz_out_len += outputHeaderSz(data_fmt);
    // isal_gzip_header_init(&gz_hdr);
    isal_deflate_init(&stream);

    stream.avail_in = header->src_sz;
    stream.next_in = header->src;
    stream.end_of_stream = 1;

    stream.flush = NO_FLUSH;
    stream.level = 2;
    stream.level_buf_size = ISAL_DEF_LVL2_DEFAULT;
    stream.level_buf = level_buf;
    stream.gzip_flag = IGZIP_GZIP_NO_HDR;

    stream.next_out = next_dest;
    stream.avail_out = header->dest_sz;

    rc = isal_deflate(&stream);
    if (ISAL_DECOMP_OK != rc) {
        QZ_ERROR("Isa-L Compress error: %d\n", rc);
        return rc;
    }
    free(level_buf);
    level_buf = NULL;

    printf("total_out = %d\n", stream.total_out);

    next_dest += stream.total_out;
    header->qz_out_len += stream.total_out;
    resl->checksum = stream.internal_state.crc;
    resl->consumed = header->src_sz;
    resl->produced = stream.total_out - outputFooterSz(data_fmt);

    outputHeaderGen(header->next_dest, resl, data_fmt);

    // header->qz_out_len = footer_pos - header->next_dest + outputFooterSz(data_fmt);
    if (NULL != header->crc32) {
        *header->crc32 = stream.internal_state.crc;
    }
    rc = QZ_OK;
    return rc;
}

int aqzISALDecompress(AQzQueueBufferList_T *buffer_data)
{
    int rc;
    AQzQueueHeader_T *header;
    struct inflate_state state;
    struct isal_gzip_header gz_hdr;
    QzDataFormat_T data_fmt;

    header = &buffer_data->header;
    data_fmt = header->data_fmt;

    isal_gzip_header_init(&gz_hdr);
    isal_inflate_init(&state);

    state.crc_flag = ISAL_GZIP_NO_HDR_VER;
    state.next_in = header->src;
    state.avail_in = header->src_sz;
    state.next_out = header->next_dest;
    state.avail_out = header->dest_sz;

    rc = isal_read_gzip_header(&state, &gz_hdr);
    if (rc != ISAL_DECOMP_OK) {
        QZ_ERROR("Isa-L Decompress read gzip header error: %d\n", rc);
        return rc;
    }

    rc = isal_inflate(&state);
    if (rc != ISAL_DECOMP_OK) {
        QZ_ERROR("Isa-L Decompress error: %d\n", rc);
        return rc;
    }
    rc = QZ_OK;
    header->qz_in_len += (outputHeaderSz(data_fmt) + header->src_sz + stdGzipFooterSz());
    header->qz_out_len = state.total_out;
    return rc;
}


int aqzCompressSw(AQzSession_T *sess, const unsigned char *src,
                             unsigned int src_len, unsigned char *dest,
                             unsigned int *dest_len)
{
    // printf("*dest = %p\n", dest);
    int rc, len = 0;
    struct isal_zstream stream;
    unsigned char *level_buf = NULL;
    unsigned char *next_dest;
    QzDataFormat_T data_fmt;
    CpaDcRqResults resl;
    QzSess_T *qz_sess;

    next_dest = dest;
    qz_sess = (QzSess_T *)(sess->internal);
    data_fmt = qz_sess->sess_params.data_fmt;

    level_buf = malloc(ISAL_DEF_LVL2_DEFAULT);
    if (NULL == level_buf) {
        return QZ_FAIL;
    }

    next_dest += outputHeaderSz(data_fmt);
    len += outputHeaderSz(data_fmt);
    isal_deflate_init(&stream);

    stream.avail_in = src_len;
    stream.next_in = (unsigned char *)src;
    stream.end_of_stream = 1;

    stream.flush = NO_FLUSH;
    stream.level = 2;
    stream.level_buf_size = ISAL_DEF_LVL2_DEFAULT;
    stream.level_buf = level_buf;
    stream.gzip_flag = IGZIP_GZIP_NO_HDR;

    stream.next_out = next_dest;
    stream.avail_out = *dest_len;

    rc = isal_deflate(&stream);
    if (ISAL_DECOMP_OK != rc) {
        QZ_ERROR("Isa-L Compress error: %d\n", rc);
        *dest_len = 0;
        return rc;
    }
    free(level_buf);
    level_buf = NULL;

    // QZ_MEMCPY(dest + outputHeaderSz(data_fmt), stream.next_out, stream.total_out, stream.total_out);
    // next_dest = dest;
    
    // printf("*dest = %p\n", dest);
    
    next_dest += stream.total_out;
    len += stream.total_out;
    resl.checksum = stream.internal_state.crc;
    resl.consumed = src_len;
    resl.produced = stream.total_out;

    outputHeaderGen(dest, &resl, data_fmt);
    aqzOutputFooterGen(next_dest, &resl, data_fmt, QZ_FUNC_BASIC);

    len += outputFooterSz(data_fmt);
    *dest_len = len;
    return rc;    
}