// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <mx/handle.h>

namespace mx {

class waitset : public handle<waitset> {
public:
    waitset() = default;

    explicit waitset(mx_handle_t value) : handle(value) {}

    explicit waitset(handle<void>&& h) : handle(h.release()) {}

    waitset(waitset&& other) : handle(other.release()) {}

    waitset& operator=(waitset&& other) {
        reset(other.release());
        return *this;
    }

    static mx_status_t create(uint32_t options, waitset* result);

    mx_status_t add(uint64_t cookie, const handle& handle,
                    mx_signals_t signals) const {
        return mx_waitset_add(get(), cookie, handle.get(), signals);
    }

    mx_status_t remove(uint64_t cookie) const {
        return mx_waitset_remove(get(), cookie);
    }

    mx_status_t wait(mx_time_t timeout, mx_waitset_result_t* results,
                     uint32_t* num_results) const {
        return mx_waitset_wait(get(), timeout, results, num_results);
    }
};

} // namespace mx
