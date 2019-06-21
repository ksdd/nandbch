/* 
 * Adapter layer for codes porting from linux-4.14.73 
 */
#ifndef _OS_SWAP_H
#define _OS_SWAP_H

#include "errno.h"
#include "fls.h"
#include "kern_levels.h"

#define printk(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)  printf(fmt, ##__VA_ARGS__)

#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define MODULE_LICENSE(x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define cpu_to_be32(x)		__bswap_32(x)
#define be32_to_cpu(x)		__bswap_32(x)
#define be16_to_cpup(x)		__bswap_16(*x)
#define cpu_to_be64(x)		__bswap_64(x)
#else
#define cpu_to_be32(x)		(x)
#define be32_to_cpu(x)		(x)
#define be16_to_cpup(x)		(*x)
#define cpu_to_be64(x)		(x)
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

typedef enum {
	GFP_KERNEL,
	GFP_ATOMIC,
	__GFP_HIGHMEM,
	__GFP_HIGH
} gfp_t;

static inline void *kmalloc(size_t size, gfp_t flags)
{
	return malloc(size);
}

static inline void *kzalloc(size_t size, gfp_t flags)
{
	void *p = malloc(size);

	if (p)
		memset(p, 0, size);

	return p;
}

static inline void kfree(void *ptr)
{
	free(ptr);
}

#endif /* _OS_SWAP_H */
