<<<<<<< HEAD
/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/bitops.h>
#undef __memset
extern void *__memset(void *, int, __kernel_size_t);
#undef __memcpy
extern void *__memcpy(void *, const void *, __kernel_size_t);
#undef __memmove
extern void *__memmove(void *, const void *, __kernel_size_t);
#undef memset
extern void *memset(void *, int, __kernel_size_t);
#undef memcpy
extern void *memcpy(void *, const void *, __kernel_size_t);
#undef memmove
=======
#include <linux/bitops.h>
extern void *__memset(void *, int, __kernel_size_t);
extern void *__memcpy(void *, const void *, __kernel_size_t);
extern void *__memmove(void *, const void *, __kernel_size_t);
extern void *memset(void *, int, __kernel_size_t);
extern void *memcpy(void *, const void *, __kernel_size_t);
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
extern void *memmove(void *, const void *, __kernel_size_t);
