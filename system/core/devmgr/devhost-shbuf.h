// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

mx_status_t devhost_shbuf_alloc(mx_device_t* dev, const ioctl_device_shbuf_alloc_args_t* args,
                                mx_handle_t* out_handle);
mx_status_t devhost_shbuf_get_flags(mx_device_t* dev, uint32_t index, mx_handle_t* out_handle);
void devhost_shbuf_close(mx_device_t* dev);
mx_status_t devhost_shbuf_queue(mx_device_t* dev, uint32_t index);
