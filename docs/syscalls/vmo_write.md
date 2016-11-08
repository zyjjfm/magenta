# mx_vmo_write

## NAME

vmo_write - write bytes to the VMO

## SYNOPSIS

```
#include <magenta/syscalls.h>

mx_status_t mx_vmo_write(mx_handle_t handle, const void* data, uint64_t offset, mx_size_t len,
        mx_size_t* actual);

```

## DESCRIPTION

**vmo_write**() attempts to write *len* bytes to a VMO at *offset*. The number of actual
bytes written is returned in *actual*.

*data* pointer to a user buffer to write bytes from.

*len* number of bytes to attempt to write.

*actual* returns the actual number of bytes written, which may be anywhere from 0 to *len*. If
a write starts beyond or extends beyond the size of the VMO, the actual bytes written will be trimmed.
Writing beyond the end of a vmo will not extend the size.

## RETURN VALUE

**mx_vmo_write**() returns **NO_ERROR** on success. In the event of failure, a negative error
value is returned.

## ERRORS

**ERR_NO_MEMORY**  Failure to allocate memory to complete write.

**ERR_INVALID_ARGS**  *actual* is an invalid pointer or NULL.

## SEE ALSO

[vmo_create](vmo_create.md),
[vmo_read](vmo_read.md),
[vmo_get_size](vmo_get_size.md),
[vmo_set_size](vmo_set_size.md),
[vmo_op_range](vmo_op_range.md).
