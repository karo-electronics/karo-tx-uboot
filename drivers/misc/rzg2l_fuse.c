// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022, Markus Bauer <MB@karo-electronics.com>
 */
#include <common.h>
#include <command.h>
#include <errno.h>
#include <fuse.h>
#include <misc.h>
#include <asm/arch/sys_proto.h>

#define RZG_SIP_OTP_READ             0x82000012
#define RZG_SIP_OTP_WRITE            0x82000013

/*
 * The 'fuse' command API
 */
int fuse_read(u32 bank, u32 word, u32 *val)
{
	unsigned long ret = 0, value = 0;

	if (bank != 0) {
		printf("Invalid bank argument, ONLY bank 0 is supported\n");
		return -EINVAL;
	}

	ret = call_rzg2l_sip_ret2(RZG_SIP_OTP_READ, (unsigned long)word, &value,0, 0);

	*val = (u32)value;

	return ret;
}

int fuse_prog(u32 bank, u32 word, u32 val)
{
	if (bank != 0) {
		printf("Invalid bank argument, ONLY bank 0 is supported\n");
		return -EINVAL;
	}

	return call_rzg2l_sip(RZG_SIP_OTP_WRITE, (unsigned long)word, (unsigned long)val, 0, 0);
}

int fuse_sense(u32 bank, u32 word, u32 *val)
{
	return fuse_read(bank, word, val);
}

int fuse_override(u32 bank, u32 word, u32 val)
{
	/* no overriding ! */
	return -EINVAL;
}
