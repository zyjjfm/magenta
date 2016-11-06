// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <magenta/types.h>
#include <mxio/io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include <magenta/hw/usb.h>
#include <magenta/device/usb.h>

int main(int argc, const char** argv) {
    if (argc < 2) return -1;

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("Error opening %s\n", argv[1]);
        return fd;
    }

    if (argc == 2) {
        int config = -1;
        int ret = ioctl_usb_get_configuration(fd, &config);
        printf("ioctl_usb_get_configuration returned %d config: %d\n", ret, config);
    } else {
        int config = atoi(argv[2]);
        if (config < 1) return -1;
    
        int ret = ioctl_usb_set_configuration(fd, &config);
        printf("ioctl_usb_set_configuration returned %d\n", ret);
    }

    close(fd);
    return 0;
}
