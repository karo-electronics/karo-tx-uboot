/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 */

#ifndef _SYS_PROTO_H_
#define _SYS_PROTO_H_

unsigned long call_rzg2l_sip(unsigned long id, unsigned long reg0,
			     unsigned long reg1, unsigned long reg2,
			     unsigned long reg3);
unsigned long call_rzg2l_sip_ret2(unsigned long id, unsigned long reg0,
				  unsigned long *reg1, unsigned long reg2,
				  unsigned long reg3);

#endif
