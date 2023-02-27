#include <stdlib.h>

#include "qatzip.h"
#include "qatzip_internal.h"
#include "qz_utils.h"

static int initQueueParams(AQzQueue_T *queue, int queue_sz)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->req = 0;
    queue->submit = 0;

    queue->queue_buffers = malloc(queue_sz * sizeof(AQzQueueBufferList_T));
    if (NULL == queue->queue_buffers) {
        goto error_mem;
    }

    queue->size = queue_sz;
    return QZ_OK;

error_mem:
    queue->size = 0;
    QZ_ERROR("initQueueParams initialized queue failed\n");
    return QZ_FAIL;
}

int aqz_queueInit(AQzQueueInstance_T *instance, int size)
{
    int rc;

    if (0 >= size) {
        rc = QZ_PARAMS;
        return rc;
    }
    rc = initQueueParams(&instance->queue_i, size);
    rc = initQueueParams(&instance->queue_o, size);
    return rc;
}

int aqz_getInQueueMemory(AQzQueue_T *queue)
{
    if (queue->queue_buffers == NULL || queue->count >= queue->size) {
        return QZ_FAIL;
    }

    return QZ_OK;
}

int aqz_getOutQueueMemory(AQzQueue_T *queue)
{
    if (queue->queue_buffers == NULL || queue->count >= queue->size) {
        return QZ_FAIL;
    }

    return QZ_OK;
}

inline int aqz_queuePush(AQzQueue_T *queue, AQzQueueBufferList_T *data)
{
    int i, rc;

    i = queue->head;

    if (queue->queue_buffers == NULL) {
        QZ_ERROR("Queue initialization failed %d/%d\n", queue->count, queue->size);
        rc = QZ_FAIL;
        return rc;
    }

    if (queue->count < queue->size) {
        queue->queue_buffers[i] = data;
        queue->head = (queue->head + 1) % queue->size;
        __sync_add_and_fetch(&queue->count, 1);
        __sync_add_and_fetch(&queue->req, 1);
        rc = QZ_OK;
    } else {
        QZ_ERROR("Queue remaining space is insufficient %d/%d\n", queue->count, queue->size);
        return AQZ_NO_SPACE;
    }
    return rc;
}

int aqz_outQueueIsEmpty(AQzQueue_T *queue)
{
    if (queue->queue_buffers == NULL) {
        return QZ_OK;
    }
    if (queue->count == 0) {
        return QZ_OK;
    }
    return QZ_FAIL;
}

int aqz_inQueueIsEmpty(AQzQueue_T *queue)
{
    if (queue->queue_buffers == NULL) {
        return QZ_OK;
    }
    if (queue->count == 0) {
        return QZ_OK;
    }
    return QZ_FAIL;
}

inline void aqz_queuePop(AQzQueue_T *queue)
{
    if (queue->queue_buffers == NULL) {
        return;
    }

    if (QZ_FAIL == aqz_inQueueIsEmpty(queue)) {
        queue->tail = (queue->tail + 1) % queue->size;
        __sync_sub_and_fetch(&queue->count, 1);
        __sync_add_and_fetch(&queue->submit, 1);
        return;
    }
    return;
}

int aqz_grabQueue(AQzQueueInstance_T *list, int num)
{
    static unsigned int pos = 0;

    if ((pos + 1) == num) {
        pos = 0;
    } else {
        pos += 1;
    }
    return pos;
}
