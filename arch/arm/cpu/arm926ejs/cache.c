/*
 * (C) Copyright 2011
 * Ilya Yanok, EmCraft Systems
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */
#include <linux/types.h>
#include <common.h>

#define DCACHE_SIZE			0x4000	  // 16KB Size of data cache in bytes
#define DCACHE_LINE_SIZE		32    // Size of a data cache line
#define DCACHE_WAYS			64    // Associativity of the cache
/*
 * This is the maximum size of an area which will be invalidated
 * using the single invalidate entry instructions.  Anything larger
 * than this, and we go for the whole cache.
 *
 * This value should be chosen such that we choose the cheapest
 * alternative.
 */
#define CACHE_DLIMIT			16384

#ifndef CONFIG_SYS_DCACHE_OFF
void invalidate_dcache_all(void)
{
	asm volatile (
		"mcr    p15, 0, %0, c7, c6, 0\n" /* invalidate d-cache */
		"mcr	p15, 0, %0, c7, c5, 0\n" /* invalidate I cache */
		"mcr	p15, 0, %0, c7, c10, 4\n" /* data write back */
		:
		: "r"(0)
		: "memory");
}

void invalidate_dcache_range(unsigned long start, unsigned long end)
{
	asm volatile (
#ifndef CONFIG_SYS_ARM_CACHE_WRITETHROUGH
		"tst	%1, %4\n"
		"mcrne	p15, 0, %1, c7, c10, 1\n" /* clean D entry */
		"tst	%2, %4\n"
		"mcrne	p15, 0, %2, c7, c10, 1\n" /* clean D entry */
#endif
		"bic	%1, %1, %4\n"
		"add	%2, %2, %4\n"
		"bic	%2, %2, %4\n"
		"1:\n"
		"mcr	p15, 0, %1, c7, c6, 1\n" /* invalidate D cache entry */
		"add	%1, %1, %3\n"
		"cmp	%1, %2\n"
		"blo	1b\n"
		"mcr	p15, 0, %0, c7, c5, 0\n" /* invalidate I cache */
		"mcr	p15, 0, %0, c7, c10, 4\n" /* data write back */
		:
		: "r"(0), "r"(start), "r"(end),
		  "I"(DCACHE_LINE_SIZE),
		  "I"(DCACHE_LINE_SIZE - 1)
		: "memory"
		);

}

#ifndef CONFIG_SYS_ARM_CACHE_WRITETHROUGH
void flush_dcache_range(unsigned long start, unsigned long end)
{
	if (end - start > CACHE_DLIMIT)
		flush_dcache_all();
	else
		flush_cache(start & ~(SZ_4K - 1), ALIGN(end, SZ_4K));
}

void flush_cache(unsigned long start, unsigned long end)
{
	asm volatile (
		"1:\n"
		"mcr	p15, 0, %1, c7, c14, 1\n" /* clean and invalidate D entry */
		"add	%1, %1, %3\n"
		"cmp	%1, %2\n"
		"blo	1b\n"
		"mcr	p15, 0, %0, c7, c5, 0\n" /* invalidate I cache */
		"mcr	p15, 0, %0, c7, c10, 4\n" /* data write back */
		:
		: "r"(0), "r"(start), "r"(end), "I"(DCACHE_LINE_SIZE)
		: "memory"
		);
}

void flush_dcache_all(void)
{
	asm volatile (
		"1:\n"
		"mrc	p15, 0, r15, c7, c14, 3\n" /* test,clean,invalidate */
		"bne	1b\n"
		"mcr	p15, 0, %0, c7, c5, 0\n" /* invalidate I cache */
		"mcr	p15, 0, %0, c7, c10, 4\n" /* data write back */
		:
		: "r"(0)
		: "memory"
		);
}
#else
void flush_dcache_range(unsigned long start, unsigned long end)
{
	invalidate_dcache_range(start, end);
}

void flush_cache(unsigned long start, unsigned long end)
{
	invalidate_dcache_range(start, end);
}

void flush_dcache_all(void)
{
	invalidate_dcache_all();
}
#endif /* CONFIG_SYS_ARM_CACHE_WRITETHROUGH */

#else /* #ifndef CONFIG_SYS_DCACHE_OFF */
void invalidate_dcache_all(void)
{
}

void flush_dcache_all(void)
{
}

void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
}

void flush_dcache_range(unsigned long start, unsigned long stop)
{
}

void  flush_cache(unsigned long start, unsigned long size)
{
}
#endif /* #ifndef CONFIG_SYS_DCACHE_OFF */

/*
 * Stub implementations for l2 cache operations
 */
void __l2_cache_disable(void)
{
}
void l2_cache_disable(void)
        __attribute__((weak, alias("__l2_cache_disable")));
