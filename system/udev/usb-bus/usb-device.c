// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/common/usb.h>
#include <ddk/protocol/usb.h>
#include <magenta/device/usb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "usb-device.h"

static void usb_iotxn_queue(mx_device_t* device, iotxn_t* txn) {
    usb_device_t* dev = get_usb_device(device);
    usb_protocol_data_t* usb_data = iotxn_pdata(txn, usb_protocol_data_t);
    usb_data->device_id = dev->device_id;

    // forward iotxn to HCI device
    iotxn_queue(dev->hci_device, txn);
}

static ssize_t usb_device_ioctl(mx_device_t* device, uint32_t op,
        const void* in_buf, size_t in_len, void* out_buf, size_t out_len) {
    usb_device_t* dev = get_usb_device(device);

    if (dev->device_type == USB_DEVICE_TYPE_INTERFACE) {
        switch (op) {
        case IOCTL_USB_GET_DEVICE_DESC:
        case IOCTL_USB_GET_CONFIG_DESC_SIZE:
        case IOCTL_USB_GET_CONFIG_DESC:
        // go ask Dad
        return device->parent->ops->ioctl(device->parent, op, in_buf, in_len, out_buf, out_len);
        default:
            break;
            // fall through to next switch statement
        }
    }

    switch (op) {
    case IOCTL_USB_GET_DEVICE_TYPE: {
        int* reply = out_buf;
        if (out_len < sizeof(*reply)) return ERR_BUFFER_TOO_SMALL;
        *reply = dev->device_type;
        return sizeof(*reply);
    }
    case IOCTL_USB_GET_DEVICE_SPEED: {
        int* reply = out_buf;
        if (out_len < sizeof(*reply)) return ERR_BUFFER_TOO_SMALL;
        *reply = dev->speed;
        return sizeof(*reply);
    }
    case IOCTL_USB_GET_DEVICE_DESC: {
        usb_device_descriptor_t* descriptor = dev->device_desc;
        if (out_len < sizeof(*descriptor)) return ERR_BUFFER_TOO_SMALL;
        memcpy(out_buf, descriptor, sizeof(*descriptor));
        return sizeof(*descriptor);
    }
    case IOCTL_USB_GET_CONFIG_DESC_SIZE: {
        usb_configuration_descriptor_t* descriptor = dev->config_descs[0];
        int* reply = out_buf;
        if (out_len < sizeof(*reply)) return ERR_BUFFER_TOO_SMALL;
        *reply = le16toh(descriptor->wTotalLength);
        return sizeof(*reply);
    }
    case IOCTL_USB_GET_CONFIG_DESC: {
        usb_configuration_descriptor_t* descriptor = dev->config_descs[0];
        size_t desc_length = le16toh(descriptor->wTotalLength);
        if (out_len < desc_length) return ERR_BUFFER_TOO_SMALL;
        memcpy(out_buf, descriptor, desc_length);
        return desc_length;
    }
    case IOCTL_USB_GET_DESCRIPTORS_SIZE: {
        size_t desc_length = 0;
        if (dev->interface_desc) {
            desc_length = dev->interface_desc_length;
        } else {
            usb_configuration_descriptor_t* descriptor = dev->config_descs[0];
            desc_length = le16toh(descriptor->wTotalLength);
        }
        int* reply = out_buf;
        if (out_len < sizeof(*reply)) return ERR_BUFFER_TOO_SMALL;
        *reply = desc_length;
        return sizeof(*reply);
    }
    case IOCTL_USB_GET_DESCRIPTORS: {
        void* descriptors = NULL;
        size_t desc_length = 0;
        if (dev->interface_desc) {
            descriptors = dev->interface_desc;
            desc_length = dev->interface_desc_length;
        } else {
            usb_configuration_descriptor_t* descriptor = dev->config_descs[0];
            descriptors = descriptor;
            desc_length = le16toh(descriptor->wTotalLength);
        }
        if (out_len < desc_length) return ERR_BUFFER_TOO_SMALL;
        memcpy(out_buf, descriptors, desc_length);
        return desc_length;
    }
    case IOCTL_USB_GET_STRING_DESC: {
        if (in_len != sizeof(int)) return ERR_INVALID_ARGS;
        if (out_len == 0) return 0;
        int id = *((int *)in_buf);
        char* string;
        mx_status_t result = usb_get_string_descriptor(device, id, &string);
        if (result < 0) return result;
        size_t length = strlen(string) + 1;
        if (length > out_len) {
            // truncate the string
            memcpy(out_buf, string, out_len - 1);
            ((char *)out_buf)[out_len - 1] = 0;
            length = out_len;
        } else {
            memcpy(out_buf, string, length);
        }
        free(string);
        return length;
    }
    default:
        return ERR_NOT_SUPPORTED;
    }
}

void usb_device_remove(usb_device_t* dev) {
    usb_device_t* child;
    while ((child = list_remove_head_type(&dev->children, usb_device_t, node)) != NULL) {
        device_remove(&child->device);
    }
    device_remove(&dev->device);
}

static mx_status_t usb_device_release(mx_device_t* device) {
    usb_device_t* dev = get_usb_device(device);

    if (dev->device_desc && dev->config_descs) {
        for (int i = 0; i < dev->device_desc->bNumConfigurations; i++) {
            free(dev->config_descs[i]);
        }
    }
    free(dev->device_desc);
    free(dev->config_descs);
    free(dev->interface_desc);
    free(dev);

    return NO_ERROR;
}

static mx_protocol_device_t usb_device_proto = {
    .iotxn_queue = usb_iotxn_queue,
    .ioctl = usb_device_ioctl,
    .release = usb_device_release,
};

static mx_driver_t _driver_usb_device BUILTIN_DRIVER = {
    .name = "usb_device",
};

static mx_status_t usb_device_add_interface(usb_device_t* parent,
                                            usb_device_descriptor_t* device_descriptor,
                                            usb_interface_descriptor_t* interface_desc,
                                            size_t interface_desc_length) {
    usb_device_t* dev = calloc(1, sizeof(usb_device_t));
    if (!dev)
        return ERR_NO_MEMORY;

    list_initialize(&dev->children);
    dev->device_type = USB_DEVICE_TYPE_INTERFACE;
    dev->hci_device = parent->hci_device;
    dev->device_id = parent->device_id;
    dev->hub_id = parent->hub_id;
    dev->speed = parent->speed;
    dev->interface_desc = interface_desc;
    dev->interface_desc_length = interface_desc_length;

    char name[20];
    snprintf(name, sizeof(name), "usb-dev-%03d-%d", parent->device_id, interface_desc->bInterfaceNumber);

    device_init(&dev->device, &_driver_usb_device, name, &usb_device_proto);
    dev->device.protocol_id = MX_PROTOCOL_USB;

    int count = 0;
    dev->props[count++] = (mx_device_prop_t){ BIND_PROTOCOL, 0, MX_PROTOCOL_USB };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_DEVICE_TYPE, 0, USB_DEVICE_TYPE_INTERFACE };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_VID, 0, device_descriptor->idVendor };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_PID, 0, device_descriptor->idProduct };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_IFC_CLASS, 0, interface_desc->bInterfaceClass };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_IFC_SUBCLASS, 0, interface_desc->bInterfaceSubClass };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_IFC_PROTOCOL, 0, interface_desc->bInterfaceProtocol };
    dev->device.props = dev->props;
    dev->device.prop_count = count;

    mx_status_t status = device_add(&dev->device, &parent->device);
    if (status == NO_ERROR) {
        list_add_head(&parent->children, &dev->node);
    } else {
        free(dev);
    }
    return status;
}

#define NEXT_DESCRIPTOR(header) ((usb_descriptor_header_t*)((void*)header + header->bLength))

static mx_status_t usb_device_add_interfaces(usb_device_t* parent,
                                             usb_device_descriptor_t* device_descriptor,
                                             usb_configuration_descriptor_t* config) {
    mx_status_t result = NO_ERROR;

    // Iterate through interfaces in first configuration and create devices for them
    usb_descriptor_header_t* header = NEXT_DESCRIPTOR(config);
    usb_descriptor_header_t* end = (usb_descriptor_header_t*)((void*)config + config->wTotalLength);

    while (header < end) {
        if (header->bDescriptorType == USB_DT_INTERFACE) {
            usb_interface_descriptor_t* intf_desc = (usb_interface_descriptor_t*)header;
            // find end of current interface descriptor
            usb_descriptor_header_t* next = NEXT_DESCRIPTOR(intf_desc);
            while (next < end) {
                if (next->bDescriptorType == USB_DT_INTERFACE) {
                    usb_interface_descriptor_t* test_intf = (usb_interface_descriptor_t*)next;
                    // include alternate interfaces in the current interface
                    if (test_intf->bAlternateSetting == 0) {
                        // found next top level interface
                        break;
                    }
                }
                next = NEXT_DESCRIPTOR(next);
            }

            size_t length = (void *)next - (void *)intf_desc;
            usb_interface_descriptor_t* intf_copy = malloc(length);
            if (!intf_copy) return ERR_NO_MEMORY;
            memcpy(intf_copy, intf_desc, length);

            mx_status_t status = usb_device_add_interface(parent, device_descriptor, intf_copy, length);
            if (status != NO_ERROR) {
                result = status;
            }

            header = next;
        } else {
            header = NEXT_DESCRIPTOR(header);
        }
    }

    return result;
}

mx_status_t usb_device_add(mx_device_t* hci_device, mx_device_t* bus_device, uint32_t device_id,
                           uint32_t hub_id, usb_speed_t speed,
                           usb_device_descriptor_t* device_descriptor,
                           usb_configuration_descriptor_t** config_descriptors,
                           usb_device_t** out_device) {

    printf("* found USB device (0x%04x:0x%04x, USB %x.%x)\n",
              device_descriptor->idVendor, device_descriptor->idProduct,
              device_descriptor->bcdUSB >> 8, device_descriptor->bcdUSB & 0xff);

    usb_device_t* dev = calloc(1, sizeof(usb_device_t));
    if (!dev)
        return ERR_NO_MEMORY;

    list_initialize(&dev->children);
    dev->device_type = USB_DEVICE_TYPE_DEVICE;
    dev->hci_device = hci_device;
    dev->device_id = device_id;
    dev->hub_id = hub_id;
    dev->speed = speed;
    dev->device_desc = device_descriptor;
    dev->config_descs = config_descriptors;

    char name[16];
    snprintf(name, sizeof(name), "usb-dev-%03d", device_id);

    device_init(&dev->device, &_driver_usb_device, name, &usb_device_proto);
    dev->device.protocol_id = MX_PROTOCOL_USB;

    int count = 0;
    dev->props[count++] = (mx_device_prop_t){ BIND_PROTOCOL, 0, MX_PROTOCOL_USB };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_DEVICE_TYPE, 0, USB_DEVICE_TYPE_DEVICE };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_VID, 0, device_descriptor->idVendor };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_PID, 0, device_descriptor->idProduct };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_CLASS, 0, device_descriptor->bDeviceClass };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_SUBCLASS, 0, device_descriptor->bDeviceSubClass };
    dev->props[count++] = (mx_device_prop_t){ BIND_USB_PROTOCOL, 0, device_descriptor->bDeviceProtocol };
    dev->device.props = dev->props;
    dev->device.prop_count = count;

    if (device_descriptor->bDeviceClass == 0) {
        // For now, do not allow binding to root of a composite device so clients will bind to
        // the child interfaces instead. We may remove this restriction later if the need arises
        // and we have a good mechanism for prioritizing which devices a driver binds to.
        device_set_bindable(&dev->device, false);
    }

    mx_status_t status = device_add(&dev->device, bus_device);
    if (status == NO_ERROR) {
        *out_device = dev;
    } else {
        free(dev);
    }

    if (status == NO_ERROR && device_descriptor->bDeviceClass == 0) {
        // add children for composite device interfaces
        status = usb_device_add_interfaces(dev, device_descriptor, config_descriptors[0]);
    }

    return status;
}
