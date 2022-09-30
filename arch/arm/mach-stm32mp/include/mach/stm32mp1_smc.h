/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (C) 2019, STMicroelectronics - All Rights Reserved
 */

#ifndef __STM32MP1_SMC_H__
#define __STM32MP1_SMC_H__

#include <linux/arm-smccc.h>

/* SMC service generic return codes */
#define STM32_SMC_OK			0x00000000U
#define STM32_SMC_NOT_SUPPORTED		0xFFFFFFFFU
#define STM32_SMC_FAILED		0xFFFFFFFEU
#define STM32_SMC_INVALID_PARAMS	0xFFFFFFFDU

/*
 * SMC function IDs for STM32 Service queries.
 * STM32 SMC services use the space between 0x82000000 and 0x8200FFFF
 * like this is defined in SMC calling Convention by ARM
 * for SiP (silicon Partner).
 * https://developer.arm.com/docs/den0028/latest
 */

/* Secure Service access from Non-secure */

/*
 * SMC function STM32_SMC_PWR.
 *
 * Argument a0: (input) SMCC ID.
 *		(output) Status return code.
 * Argument a1: (input) Service ID (STM32_SMC_REG_xxx).
 * Argument a2: (input) Register offset or physical address.
 *		(output) Register read value, if applicable.
 * Argument a3: (input) Register target value if applicable.
 */
#define STM32_SMC_PWR			0x82001001

/*
 * SMC functions STM32_SMC_BSEC.
 *
 * Argument a0: (input) SMCC ID.
 *		(output) Status return code.
 * Argument a1: (input) Service ID (STM32_SMC_READ_xxx/_PROG_xxx/_WRITE_xxx).
 *		(output) OTP read value, if applicable.
 * Argument a2: (input) OTP index.
 * Argument a3: (input) OTP value if applicable.
 */
#define STM32_SMC_BSEC			0x82001003

/* Service ID for STM32_SMC_PWR */
#define STM32_SMC_REG_READ		0x0
#define STM32_SMC_REG_WRITE		0x1
#define STM32_SMC_REG_SET		0x2
#define STM32_SMC_REG_CLEAR		0x3

/* Service ID for STM32_SMC_BSEC */
#define STM32_SMC_READ_SHADOW		0x01
#define STM32_SMC_PROG_OTP		0x02
#define STM32_SMC_WRITE_SHADOW		0x03
#define STM32_SMC_READ_OTP		0x04
#define STM32_SMC_READ_ALL		0x05
#define STM32_SMC_WRITE_ALL		0x06
#define STM32_SMC_WRLOCK_OTP		0x07

#define stm32_smc_exec(svc, op, data1, data2) \
	stm32_smc(svc, op, data1, data2, NULL)

#ifdef CONFIG_ARM_SMCCC
static inline u32 stm32_smc(u32 svc, u8 op, u32 data1, u32 data2, u32 *result)
{
	struct arm_smccc_res res;

	arm_smccc_smc(svc, op, data1, data2, 0, 0, 0, 0, &res);

	if (res.a0) {
		pr_err("%s: Failed to exec svc=%x op=%x in secure mode (err = %ld)\n",
		       __func__, svc, op, res.a0);
		return -EINVAL;
	}
	if (result)
		*result = (u32)res.a1;

	return 0;
}
#else
static inline u32 stm32_smc(u32 svc, u8 op, u32 data1, u32 data2, u32 *result)
{
	return 0;
}
#endif

#endif /* __STM32MP1_SMC_H__ */
