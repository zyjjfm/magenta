// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <magenta/device/ioctl.h>
#include <magenta/device/ioctl-wrapper.h>
#include <magenta/types.h>

// Bind to a driver
//   in: driver to bind to (optional)
//   out: none
#define IOCTL_DEVICE_BIND \
    IOCTL(IOCTL_KIND_DEFAULT, IOCTL_FAMILY_DEVICE, 0)

// Watch a directory for changes
//   in: none
//   out: handle to msgpipe to get notified on
#define IOCTL_DEVICE_WATCH_DIR \
    IOCTL(IOCTL_KIND_GET_HANDLE, IOCTL_FAMILY_DEVICE, 1)

// Return a handle to the device event
//   in: none
//   out: handle
#define IOCTL_DEVICE_GET_EVENT_HANDLE \
    IOCTL(IOCTL_KIND_GET_HANDLE, IOCTL_FAMILY_DEVICE, 2)

// Return driver name string
//   in: none
//   out: null-terminated string
#define IOCTL_DEVICE_GET_DRIVER_NAME \
    IOCTL(IOCTL_KIND_DEFAULT, IOCTL_FAMILY_DEVICE, 3)

// Return device name string
//   in: none
//   out: null-terminated string
#define IOCTL_DEVICE_GET_DEVICE_NAME \
    IOCTL(IOCTL_KIND_DEFAULT, IOCTL_FAMILY_DEVICE, 4)

// Suspends the device
// (intended for driver suspend/resume testing)
//   in: none
//   out: none
#define IOCTL_DEVICE_DEBUG_SUSPEND \
    IOCTL(IOCTL_KIND_DEFAULT, IOCTL_FAMILY_DEVICE, 5)

// Resumes the device
// (intended for driver suspend/resume testing)
//   in: none
//   out: none
#define IOCTL_DEVICE_DEBUG_RESUME \
    IOCTL(IOCTL_KIND_DEFAULT, IOCTL_FAMILY_DEVICE, 6)

// Buffer indices for IOCTL_DEVICE_ALLOC_SHBUF
enum {
    DEVICE_SHBUF_READ = 0,
    DEVICE_SHBUF_WRITE = 1,
};

typedef struct {
    size_t packet_size;   // packet size for buffer
    uint32_t packet_count;  // number of packets in the buffer
    uint32_t index;         // index of buffer to allocate
} ioctl_device_shbuf_alloc_args_t;

typedef struct {
    // set bit to one to queue a packet
    uint64_t    queue_bits;
    // set bit to one to mark a packet done
    uint64_t    done_bits;
    // return code
    mx_status_t status;
} mx_device_shbuf_flags_t;

// Allocates a shared buffer for a driver
//   in: ioctl_device_shbuf_alloc_args_t
//   out: handle for buffer VMO
#define IOCTL_DEVICE_SHBUF_ALLOC \
    IOCTL(IOCTL_KIND_GET_HANDLE, IOCTL_FAMILY_DEVICE, 7)

// Retrieves the buffer flags VMO for a shared buffer
//   in: buffer index
//   out: handle for buffer flags VMO
#define IOCTL_DEVICE_SHBUF_GET_FLAGS \
    IOCTL(IOCTL_KIND_GET_HANDLE, IOCTL_FAMILY_DEVICE, 8)

// Queues packets in a shared buffer
//   in: buffer index
//   out: none
#define IOCTL_DEVICE_SHBUF_QUEUE \
    IOCTL(IOCTL_KIND_DEFAULT, IOCTL_FAMILY_DEVICE, 9)

// Indicates if there's data available to read,
// or room to write, or an error condition.
#define DEVICE_SIGNAL_READABLE MX_USER_SIGNAL_0
#define DEVICE_SIGNAL_WRITABLE MX_USER_SIGNAL_1
#define DEVICE_SIGNAL_ERROR MX_USER_SIGNAL_2

// ssize_t ioctl_device_bind(int fd, const char* in, size_t in_len);
IOCTL_WRAPPER_VARIN(ioctl_device_bind, IOCTL_DEVICE_BIND, char);

// ssize_t ioctl_device_watch_dir(int fd, mx_handle_t* out);
IOCTL_WRAPPER_OUT(ioctl_device_watch_dir, IOCTL_DEVICE_WATCH_DIR, mx_handle_t);

// ssize_t ioctl_device_get_event_handle(int fd, mx_handle_t* out);
IOCTL_WRAPPER_OUT(ioctl_device_get_event_handle, IOCTL_DEVICE_GET_EVENT_HANDLE, mx_handle_t);

// ssize_t ioctl_device_get_driver_name(int fd, char* out, size_t out_len);
IOCTL_WRAPPER_VAROUT(ioctl_device_get_driver_name, IOCTL_DEVICE_GET_DRIVER_NAME, char);

// ssize_t ioctl_device_get_device_name(int fd, char* out, size_t out_len);
IOCTL_WRAPPER_VAROUT(ioctl_device_get_device_name, IOCTL_DEVICE_GET_DEVICE_NAME, char);

// ssize_t ioctl_device_debug_suspend(int fd);
IOCTL_WRAPPER(ioctl_device_debug_suspend, IOCTL_DEVICE_DEBUG_SUSPEND);

// ssize_t ioctl_device_debug_resume(int fd);
IOCTL_WRAPPER(ioctl_device_debug_resume, IOCTL_DEVICE_DEBUG_RESUME);

// ssize_t ioctl_device_shbuf_alloc(int fd, ioctl_device_shbuf_alloc_args_t* args, mx_handle_t* out);
IOCTL_WRAPPER_INOUT(ioctl_device_shbuf_alloc, IOCTL_DEVICE_SHBUF_ALLOC,
                    ioctl_device_shbuf_alloc_args_t, mx_handle_t);

// ssize_t ioctl_device_shbuf_get_flags(int fd, uint32_t index, mx_handle_t* out);
IOCTL_WRAPPER_INOUT(ioctl_device_shbuf_get_flags, IOCTL_DEVICE_SHBUF_GET_FLAGS, uint32_t, mx_handle_t);

// ssize_t ioctl_device_shbuf_queue(int fd, uint32_t index, mx_handle_t* out);
IOCTL_WRAPPER_IN(ioctl_device_shbuf_queue, IOCTL_DEVICE_SHBUF_QUEUE, uint32_t);
