# mx_vmo_read

## NAME

vmo_read - read bytes from the VMO

## SYNOPSIS

```
#include <magenta/syscalls.h>

mx_status_t mx_vmo_read(mx_handle_t handle, void* data, uint64_t offset, mx_size_t len, mx_size_t* actual);

```

## DESCRIPTION

**vmo_read**() attempts to read *len* bytes from a VMO at *offset*. The number of actual
bytes read is returned in *actual*.

*data* pointer to a user buffer to read bytes into.

*len* number of bytes to attempt to read. *data* buffer should be large enough for at least this
many bytes.

*actual* returns the actual number of bytes read, which may be anywhere from 0 to *len*. If
a read starts beyond or extends beyond the size of the VMO, the actual bytes read will be trimmed.

## RETURN VALUE

**mx_vmo_read**() returns **NO_ERROR** on success. In the event of failure, a negative error
value is returned.

## ERRORS

**ERR_INVALID_ARGS**  *actual* is an invalid pointer or NULL.

## SEE ALSO

[vmo_create](vmo_create.md),
[vmo_write](vmo_write.md),
[vmo_get_size](vmo_get_size.md),
[vmo_set_size](vmo_set_size.md),
[vmo_op_range](vmo_op_range.md).
