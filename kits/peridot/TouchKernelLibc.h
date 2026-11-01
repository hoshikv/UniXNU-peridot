/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef PERIDOT_TOUCH_KERNEL_LIBC_H
#define PERIDOT_TOUCH_KERNEL_LIBC_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS
int printf(const char *, ...) __printflike(1, 2);
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
__END_DECLS

#endif
