// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/iotxn.h>
#include <ddk/device.h>
#include <magenta/syscalls.h>
#include <sys/param.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

#define TRACE 0

#if TRACE
#define xprintf(fmt...) printf(fmt)
#else
#define xprintf(fmt...) \
    do {                \
    } while (0)
#endif

#define IOTXN_FLAG_CLONE (1 << 0)
#define IOTXN_FLAG_FREE  (1 << 1)   // for double-free checking

typedef struct iotxn_priv iotxn_priv_t;

struct iotxn_priv {
    // data payload.
    io_buffer_t buffer;
    mx_off_t buffer_offset;

    uint32_t flags;

    // payload size
    mx_size_t data_size;
    // extra data, at the end of this ioxtn_t structure
    mx_size_t extra_size;

    iotxn_t txn; // must be at the end for extra data, only valid if not a clone
};

#define get_priv(iotxn) containerof(iotxn, iotxn_priv_t, txn)

static list_node_t free_list = LIST_INITIAL_VALUE(free_list);
static list_node_t clone_list = LIST_INITIAL_VALUE(clone_list); // free list for clones
static mtx_t free_list_mutex = MTX_INIT;
static mtx_t clone_list_mutex = MTX_INIT;

static void iotxn_complete(iotxn_t* txn, mx_status_t status, mx_off_t actual) {
    txn->actual = actual;
    txn->status = status;
    if (txn->complete_cb) {
        txn->complete_cb(txn, txn->cookie);
    }
}

static void iotxn_copyfrom(iotxn_t* txn, void* data, size_t length, size_t offset) {
    iotxn_priv_t* priv = get_priv(txn);
    size_t count = MIN(length, priv->data_size - offset);
    memcpy(data, io_buffer_virt(&priv->buffer) + priv->buffer_offset + offset, count);
}

static void iotxn_copyto(iotxn_t* txn, const void* data, size_t length, size_t offset) {
    iotxn_priv_t* priv = get_priv(txn);
    size_t count = MIN(length, priv->data_size - offset);
    memcpy(io_buffer_virt(&priv->buffer) + priv->buffer_offset + offset, data, count);
}

static void iotxn_physmap(iotxn_t* txn, mx_paddr_t* addr) {
    iotxn_priv_t* priv = get_priv(txn);
    *addr = priv->buffer.phys;
}

static void iotxn_mmap(iotxn_t* txn, void** data) {
    iotxn_priv_t* priv = get_priv(txn);
    *data = io_buffer_virt(&priv->buffer) + priv->buffer_offset;
}

static iotxn_priv_t* iotxn_get_clone(size_t extra_size) {
    iotxn_priv_t* priv = NULL;
    iotxn_t* clone = NULL;
   // look in clone list first for something that fits
    bool found = false;

    mtx_lock(&clone_list_mutex);
    list_for_every_entry (&clone_list, clone, iotxn_t, node) {
        priv = get_priv(clone);
        if (priv->extra_size >= extra_size) {
            found = true;
            break;
        }
    }
    // found one that fits, skip allocation
    if (found) {
        list_delete(&clone->node);
        priv->buffer_offset = 0;
        priv->flags &= ~IOTXN_FLAG_FREE;
        if (priv->extra_size) memset(&priv[1], 0, priv->extra_size);
        mtx_unlock(&clone_list_mutex);
        goto out;
    }
    mtx_unlock(&clone_list_mutex);

    // didn't find one that fits, allocate a new one
    priv = calloc(1, sizeof(iotxn_priv_t) + extra_size);
    if (!priv) {
        xprintf("iotxn: out of memory\n");
        return NULL;
    }

out:
    priv->flags |= IOTXN_FLAG_CLONE;
    return priv;
}

static mx_status_t iotxn_clone(iotxn_t* txn, iotxn_t** out, size_t extra_size) {
    iotxn_priv_t* priv = get_priv(txn);
    iotxn_priv_t* cpriv = iotxn_get_clone(extra_size);
    if (!cpriv) return ERR_NO_MEMORY;

    // copy data payload metadata to the clone so the api can just work
    memcpy(&cpriv->buffer, &priv->buffer, sizeof(priv->buffer));
    cpriv->data_size = priv->data_size;
    memcpy(&cpriv->txn, txn, sizeof(iotxn_t));
    cpriv->txn.complete_cb = NULL; // clear the complete cb
    *out = &cpriv->txn;
    return NO_ERROR;
}

static void iotxn_release(iotxn_t* txn) {
    xprintf("iotxn_release: txn=%p\n", txn);
    iotxn_priv_t* priv = get_priv(txn);
    if (priv->flags & IOTXN_FLAG_FREE) {
        printf("double free in iotxn_release\n");
        abort();
    }

    if (priv->flags & IOTXN_FLAG_CLONE) {
        mtx_lock(&clone_list_mutex);
        list_add_tail(&clone_list, &txn->node);
        priv->flags |= IOTXN_FLAG_FREE;
        mtx_unlock(&clone_list_mutex);
    } else {
        mtx_lock(&free_list_mutex);
        list_add_tail(&free_list, &txn->node);
        priv->flags |= IOTXN_FLAG_FREE;
        mtx_unlock(&free_list_mutex);
    }
}

static iotxn_ops_t ops = {
    .complete = iotxn_complete,
    .copyfrom = iotxn_copyfrom,
    .copyto = iotxn_copyto,
    .physmap = iotxn_physmap,
    .mmap = iotxn_mmap,
    .clone = iotxn_clone,
    .release = iotxn_release,
};

mx_status_t iotxn_alloc(iotxn_t** out, uint32_t flags, size_t data_size, size_t extra_size) {
    xprintf("iotxn_alloc: flags=0x%x data_size=0x%zx extra_size=0x%zx\n", flags, data_size, extra_size);
    iotxn_t* txn = NULL;
    iotxn_priv_t* priv = NULL;
    // look in free list first for something that fits
    bool found = false;

    mtx_lock(&free_list_mutex);
    list_for_every_entry (&free_list, txn, iotxn_t, node) {
        priv = get_priv(txn);
        if (priv->buffer.size >= data_size && priv->extra_size >= extra_size) {
            found = true;
            break;
        }
    }
    // found one that fits, skip allocation
    if (found) {
        list_delete(&txn->node);
        memset(&txn, 0, sizeof(iotxn_t));
        memset(io_buffer_virt(&priv->buffer), 0, priv->buffer.size);
        priv->flags &= ~IOTXN_FLAG_FREE;
        mtx_unlock(&free_list_mutex);
        goto out;
    }
    mtx_unlock(&free_list_mutex);

    // didn't find one that fits, allocate a new one
    priv = calloc(1, sizeof(iotxn_priv_t) + extra_size);
    if (!priv) return ERR_NO_MEMORY;
    if (data_size > 0) {
        mx_status_t status = io_buffer_init(&priv->buffer, data_size,
                                            MX_VM_FLAG_PERM_READ | MX_VM_FLAG_PERM_WRITE);
        if (status != NO_ERROR) {
            free(priv);
            return status;
        }
    }

    // layout is iotxn_priv_t | extra_size
    priv->extra_size = extra_size;
out:
    priv->data_size = data_size;
    priv->txn.ops = &ops;
    *out = &priv->txn;
    xprintf("iotxn_alloc: found=%d txn=%p buffer_size=0x%zx\n", found, &priv->txn, priv->buffer.size);
    return NO_ERROR;
}

mx_status_t iotxn_alloc_from_buffer(iotxn_t** out, io_buffer_t* buffer, size_t data_size, 
                                    mx_off_t data_offset, size_t extra_size) {                                
    iotxn_priv_t* priv = iotxn_get_clone(extra_size);
    if (!priv) return ERR_NO_MEMORY;

    memcpy(&priv->buffer, buffer, sizeof(priv->buffer));
    priv->data_size = data_size;
    priv->buffer_offset = data_offset;

    *out = &priv->txn;
    return NO_ERROR;
}

void iotxn_queue(mx_device_t* dev, iotxn_t* txn) {
    dev->ops->iotxn_queue(dev, txn);
}
