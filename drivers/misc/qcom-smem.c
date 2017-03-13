/*
 * Copyright (C) 2017, Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * based on: platform/msm_shared/smem.c
 * from Android Little Kernel: https://github.com/littlekernel/lk
 *
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <common.h>
#include <asm/io.h>

#include "qcom-smem.h"

static struct smem *smem;

/* DYNAMIC SMEM REGION feature enables LK to dynamically
 * read the SMEM addr info from TCSR register or IMEM location.
 * The first word read, if indicates a MAGIC number, then
 * Dynamic SMEM is assumed to be enabled. Read the remaining
 * SMEM info for SMEM Size and Phy_addr from the other bytes.
 */

void *smem_get_base_addr(void)
{
	unsigned long smem_base = 0;

	if (smem)
		return smem;

#ifdef TCSR_TZ_WONCE
	struct smem_addr_info *smem_info = (void *)TCSR_TZ_WONCE;

	if (smem_info && smem_info->identifier == SMEM_TARGET_INFO_IDENTIFIER)
		smem_base = smem_info->phy_addr;
#endif
	if (!smem_base)
		smem_base = MSM_SHARED_BASE;

	smem = (void *)smem_base;
	return smem;
}

/* buf MUST be 4byte aligned, and len MUST be a multiple of 8. */
unsigned smem_read_alloc_entry(smem_mem_type_t type, void *buf, int len)
{
	struct smem_alloc_info *ainfo;
	uint32_t *dest = buf;
	void *src;
	size_t size;
	void *smem_addr = smem_get_base_addr();

	if (((len & 0x3) != 0) || (((unsigned long)buf & 0x3) != 0))
		return 1;

	if (type < SMEM_FIRST_VALID_TYPE || type > SMEM_LAST_VALID_TYPE)
		return 1;

	/* TODO: Use smem spinlocks */
	ainfo = &smem->alloc_info[type];
	if (readl(&ainfo->allocated) == 0)
		return 1;

	size = readl(&ainfo->size);
	if (size < ALIGN((size_t)len, 8))
		return 1;

	src = smem_addr + readl(&ainfo->offset);
	for (; len > 0; src += 4, len -= 4)
		*dest++ = readl(src);

	return 0;
}

/* Return a pointer to smem_item with size */
void *smem_get_alloc_entry(smem_mem_type_t type, uint32_t* size)
{
	struct smem_alloc_info *ainfo;
	void *base_ext;
	uint32_t offset;
	void *smem_addr = smem_get_base_addr();

	if (type < SMEM_FIRST_VALID_TYPE || type > SMEM_LAST_VALID_TYPE)
		return NULL;

	ainfo = &smem->alloc_info[type];
	if (readl(&ainfo->allocated) == 0)
		return NULL;

	*size = readl(&ainfo->size);
	base_ext = (void *)(unsigned long)readl(&ainfo->base_ext);
	offset = readl(&ainfo->offset);

	return (base_ext ?: smem_addr) + offset;
}

unsigned smem_read_alloc_entry_offset(smem_mem_type_t type, void *buf, int len,
			     int offset)
{
	struct smem_alloc_info *ainfo;
	uint32_t *dest = buf;
	void *src;
	size_t size = len;
	void *smem_addr = smem_get_base_addr();

	if (((len & 0x3) != 0) || (((unsigned long)buf & 0x3) != 0))
		return 1;

	if (type < SMEM_FIRST_VALID_TYPE || type > SMEM_LAST_VALID_TYPE)
		return 1;

	ainfo = &smem->alloc_info[type];
	if (readl(&ainfo->allocated) == 0)
		return 1;

	src = smem_addr + readl(&ainfo->offset) + offset;
	for (; size > 0; src += 4, size -= 4)
		*dest++ = readl(src);

	return 0;
}
