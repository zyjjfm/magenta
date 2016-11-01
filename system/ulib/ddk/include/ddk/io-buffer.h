// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <magenta/compiler.h>
#include <magenta/types.h>

#include <stdbool.h>
#include <stddef.h>

__BEGIN_CDECLS;

typedef struct {
    mx_handle_t vmo_handle;
    size_t size;
    mx_off_t offset;
    void* virt;
    mx_paddr_t phys;
} io_buffer_t;

mx_status_t io_buffer_init(io_buffer_t* buffer, size_t size, uint32_t flags);
mx_status_t io_buffer_init_vmo(io_buffer_t* buffer, mx_handle_t vmo_handle, mx_off_t offset,
                               uint32_t flags);
void io_buffer_free(io_buffer_t* buffer);

inline bool io_buffer_is_valid(io_buffer_t* buffer) {
    return (buffer->vmo_handle != MX_HANDLE_INVALID);
}

inline void* io_buffer_virt(io_buffer_t* buffer) {
    return buffer->virt + buffer->offset;
}

inline mx_paddr_t io_buffer_phys(io_buffer_t* buffer) {
    return buffer->phys + buffer->offset;
}

__END_CDECLS;
