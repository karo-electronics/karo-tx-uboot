/*
 * Copyright (C) 2017, Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * based on: platform/msm_shared/include/rpm-smd.h
 * from Android Little Kernel: https://github.com/littlekernel/lk
 *
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Fundation, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __PMIC_QCOM_SMD_RPM_H
#define __PMIC_QCOM_SMD_RPM_H

#define KEY_SOFTWARE_ENABLE		0x6E657773 // swen - software enable
#define KEY_LDO_SOFTWARE_MODE		0X646D736C // lsmd - LDO software mode
#define KEY_SMPS_SOFTWARE_MODE		0X646D7373 // ssmd - SMPS software mode
#define KEY_PIN_CTRL_ENABLE		0x6E656370 //pcen - pin control enable
#define KEY_PIN_CTRL_POWER_MODE		0x646d6370 // pcmd - pin control mode
#define KEY_CURRENT			0x616D //ma
#define KEY_MICRO_VOLT			0x7675 //uv
#define KEY_FREQUENCY			0x71657266 //freq
#define KEY_FREQUENCY_REASON		0x6E736572 //resn
#define KEY_FOLLOW_QUIET_MODE		0x6D71 //qm
#define KEY_HEAD_ROOM			0x7268 // hr
#define KEY_PIN_CTRL_CLK_BUFFER_ENABLE_KEY 0x62636370 // pccb - clk buffer pin control
#define KEY_BYPASS_ALLOWED_KEY		0x61707962 //bypa - bypass allowed
#define KEY_CORNER_LEVEL_KEY		0x6E726F63 // corn - coner voltage
#define KEY_ACTIVE_FLOOR		0x636676
#define GENERIC_DISABLE			0
#define GENERIC_ENABLE			1
#define SW_MODE_LDO_IPEAK		1
#define LDOA_RES_TYPE			0x616F646C //aodl
#define SMPS_RES_TYPE			0x61706D73 //apms

#ifdef CONFIG_QCOM_SMD_RPM
typedef enum {
	RPM_REQUEST_TYPE,
	RPM_CMD_TYPE,
	RPM_SUCCESS_REQ_ACK,
	RPM_SUCCESS_CMD_ACK,
	RPM_ERROR_ACK,
} msg_type;

enum {
	RESOURCETYPE,
	RESOURCEID,
	KVP_KEY,
	KVP_LENGTH,
	KVP_VALUE,
};

typedef struct {
	uint32_t type;
	uint32_t len;
} rpm_gen_hdr;

typedef struct {
	uint32_t key;
	uint32_t len;
	uint32_t val;
} kvp_data;

typedef struct {
	uint32_t id;
	uint32_t set;
	uint32_t resourceType;
	uint32_t resourceId;
	uint32_t dataLength;
} rpm_req_hdr;

typedef struct {
	rpm_gen_hdr hdr;
	rpm_req_hdr req_hdr;
	uint32_t payload[];
} rpm_req;

typedef struct {
	rpm_gen_hdr hdr;
	kvp_data *data;
} rpm_cmd;

typedef struct {
	rpm_gen_hdr hdr;
	uint32_t id;
	uint32_t len;
	uint32_t seq;
} rpm_ack_msg;

int rpm_send_data(uint32_t *data, uint32_t len, msg_type type);
uint32_t rpm_recv_data(uint32_t *data, uint32_t *len);
void rpm_clk_enable(uint32_t *data, uint32_t len);
void rpm_clk_disable(uint32_t *data, uint32_t len);
void smd_rpm_init(void);
void smd_rpm_uninit(void);
#else /* !CONFIG_QCOM_SMD_RPM */
static inline void smd_rpm_init(void)
{
}

static inline void smd_rpm_uninit(void)
{
}
#endif /* !CONFIG_QCOM_SMD_RPM */

#endif /* __PMIC_QCOM_SMD_RPM_H */
