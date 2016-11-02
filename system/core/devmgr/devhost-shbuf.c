// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/device.h>
#include <ddk/io-buffer.h>
#include <magenta/types.h>
#include <stdlib.h>
#include <stdio.h>

#include "devhost-shbuf.h"

#define MAX_SHBUF_INDEX     1
#define MAX_PACKET_COUNT    64

static const uint32_t shbuf_perms[MAX_SHBUF_INDEX + 1] = {
    MX_VM_FLAG_PERM_READ,                           // DEVICE_SHBUF_READ
    MX_VM_FLAG_PERM_READ | MX_VM_FLAG_PERM_WRITE,   // DEVICE_SHBUF_WRITE
};

typedef struct  {
    io_buffer_t buffer;
    io_buffer_t buffer_flags;
    size_t packet_size;   // packet size for buffer
    uint32_t packet_count;  // number of packets in the buffer
} mx_shbuf_t;

struct mx_device_shbuf {
    mx_shbuf_t buffers[MAX_SHBUF_INDEX + 1];
};

mx_status_t devhost_shbuf_alloc(mx_device_t* dev, const ioctl_device_shbuf_alloc_args_t* args,
                                mx_handle_t* out_handle) {
    uint32_t index = args->index;
    if (index > MAX_SHBUF_INDEX) {
        printf("devhost_shbuf_alloc index %d out of range\n", index);
        return ERR_INVALID_ARGS;
    }
    if (args->packet_count > MAX_PACKET_COUNT) {
        printf("devhost_shbuf_alloc packet ount %d out of range (max is %d)\n", args->packet_count,
               MAX_PACKET_COUNT);
        return ERR_INVALID_ARGS;
    }

    // FIXME (voydanoff) not thread safe
    mx_device_shbuf_t* shbuf = dev->shbuf;
    if (!shbuf) {
        shbuf = calloc(1, sizeof(struct mx_device_shbuf));
        if (!shbuf) return ERR_NO_MEMORY;
        dev->shbuf = shbuf;
    }

    mx_shbuf_t* buffer = &shbuf->buffers[index];
    if (io_buffer_is_valid(&buffer->buffer)) {
        printf("devhost_shbuf_alloc buffer already exists\n");
        return ERR_ALREADY_EXISTS;
    }

    // allocate buffer
    size_t vmo_size = args->packet_size * args->packet_count;
    uint32_t flags = shbuf_perms[index];
    mx_status_t status = io_buffer_init(&buffer->buffer, vmo_size, flags);
    if (status != NO_ERROR) return status;

    // allocate buffer flags
    status = io_buffer_init(&buffer->buffer_flags, sizeof(mx_device_shbuf_flags_t),
                            MX_VM_FLAG_PERM_READ | MX_VM_FLAG_PERM_WRITE);
    if (status != NO_ERROR) {
        io_buffer_free(&buffer->buffer);
        return status;
    }
    buffer->packet_size = args->packet_size;
    buffer->packet_count = args->packet_count;

    *out_handle = buffer->buffer.vmo_handle;
    return NO_ERROR;
}

mx_status_t devhost_shbuf_get_flags(mx_device_t* dev, uint32_t index, mx_handle_t* out_handle) {
    if (index > MAX_SHBUF_INDEX) {
        printf("devhost_shbuf_get_flags index %d out of range\n", index);
        return ERR_INVALID_ARGS;
    }

    mx_device_shbuf_t* shbuf = dev->shbuf;
    if (!shbuf) return ERR_BAD_STATE;
    mx_shbuf_t* buffer = &shbuf->buffers[index];
    mx_handle_t result = buffer->buffer_flags.vmo_handle;
    if (!result) return ERR_BAD_STATE;

    *out_handle = result;
    return NO_ERROR;
}

void devhost_shbuf_close(mx_device_t* dev) {
    mx_device_shbuf_t* shbuf = dev->shbuf;
    if (!shbuf) return;

    for (uint i = 0; i < countof(shbuf->buffers); i++) {
        io_buffer_free(&shbuf->buffers[i].buffer);
        io_buffer_free(&shbuf->buffers[i].buffer_flags);
    }
}

static mx_status_t devhost_shbuf_do_queue(mx_device_t* dev, uint32_t index) {
    mx_shbuf_t* buffer = &dev->shbuf->buffers[index];

    iotxn_t* txn;
    mx_status_t status = iotxn_alloc_from_buffer(&txn, &buffer->buffer, buffer->packet_size,
                                                 buffer->packet_size * index, 0);
    if (status != NO_ERROR) return status;

    switch (index) {
        case DEVICE_SHBUF_READ:
            txn->opcode = IOTXN_OP_READ;
            break;
        case DEVICE_SHBUF_WRITE:
            txn->opcode = DEVICE_SHBUF_WRITE;
            break;
        default:
            txn->ops->release(txn);
            return ERR_INVALID_ARGS;
    }
    txn->offset = 0;
    txn->length = buffer->packet_size;
    dev->ops->iotxn_queue(dev, txn);

    return NO_ERROR;
}

mx_status_t devhost_shbuf_queue(mx_device_t* dev, uint32_t index) {
    if (index > MAX_SHBUF_INDEX) {
        printf("devhost_shbuf_queue index %d out of range\n", index);
        return ERR_INVALID_ARGS;
    }
    mx_device_shbuf_t* shbuf = dev->shbuf;
    if (!shbuf) {
        return ERR_BAD_STATE;
    }
    mx_shbuf_t* buffer = &shbuf->buffers[index];
    if (!io_buffer_is_valid(&buffer->buffer) || !io_buffer_is_valid(&buffer->buffer_flags)) {
        return ERR_BAD_STATE;
    }

    mx_device_shbuf_flags_t* flags = (mx_device_shbuf_flags_t *)io_buffer_virt(&buffer->buffer_flags);
    printf("flags %p\n", flags);

    uint64_t queue_bits = flags->queue_bits;

    uint32_t packet;
    while ((packet = __builtin_ffsll(queue_bits)) != 0) {
        // adjust for result being 1-based
        packet--;
        if (packet >= buffer->packet_count) {
            // ignore bits set beyond packet_count
            break;
        }
        queue_bits &= (1 << packet);
        mx_status_t status = devhost_shbuf_do_queue(dev, index);
        if (status != NO_ERROR) {
            return status;
        }
    }

    return NO_ERROR;
}
