/*
 * Copyright (C) 2017, Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * based on: platform/msm_shared/rpm-smd.c
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

#include <common.h>
#include <malloc.h>
#include <mach/qcom-smd.h>
#include <power/pmic-qcom-smd-rpm.h>

#define RPM_REQ_MAGIC 0x00716572
#define RPM_CMD_MAGIC 0x00646d63
#define REQ_MSG_LENGTH 0x14
#define CMD_MSG_LENGTH 0x08
#define ACK_MSG_LENGTH 0x0C

static uint32_t msg_id;
static smd_channel_info_t ch;

void smd_rpm_init(void)
{
	smd_init(&ch, SMD_APPS_RPM);
}

void smd_rpm_uninit(void)
{
	smd_uninit(&ch);
}

static void fill_kvp_object(kvp_data **kdata, uint32_t *data, uint32_t len)
{
	*kdata = memalign(CONFIG_SYS_CACHELINE_SIZE,
			ALIGN(len, CONFIG_SYS_CACHELINE_SIZE));
	if (!*kdata)
		hang();

	memcpy(*kdata, data, len);
}

static int rpm_read_ack(void)
{
	int ret;
	rpm_ack_msg ack;
	msg_type type;
	uint32_t len = sizeof(ack);

	ret = smd_read(&ch, &len, SMD_APPS_RPM, (void *)&ack);
	if (ret)
		return ret;
	if (len == 0) {
		printf("Failed to read ACK from RPM\n");
		return -EIO;
	}

	invalidate_dcache_range((unsigned long)&ack, sizeof(ack.hdr));

	if (ack.hdr.type == RPM_CMD_MAGIC) {
		type = RPM_CMD_TYPE;
	} else if (ack.hdr.type == RPM_REQ_MAGIC) {
		type = RPM_REQUEST_TYPE;
	} else {
		printf("Invalid RPM Header type %02x\n",
			ack.hdr.type);
		return -EINVAL;
	}

	if (type == RPM_CMD_TYPE && ack.hdr.len == ACK_MSG_LENGTH) {
		debug("Received SUCCESS CMD ACK\n");
	} else if (type == RPM_REQUEST_TYPE && ack.hdr.len == ACK_MSG_LENGTH) {
		debug("Received SUCCESS REQ ACK\n");
	} else {
		printf("Received ERROR ACK: type %02x len %04x\n",
			type, ack.hdr.len);
		return -ENXIO;
	}
	debug("Read %u bytes from SMD\n", len);
	return 0;
}

int rpm_send_data(uint32_t *data, uint32_t len, msg_type type)
{
	rpm_req *req;
	rpm_cmd cmd;
	uint32_t len_to_smd;
	int ret;

	debug("%s@%d: sending RPM request %02x %p len %u\n",
		__func__, __LINE__, type, data, len);

	switch (type) {
	case RPM_REQUEST_TYPE:
		req = memalign(CONFIG_SYS_CACHELINE_SIZE,
			ALIGN(len - 8 + sizeof(*req),
				CONFIG_SYS_CACHELINE_SIZE));
		if (req == NULL)
			return -ENOMEM;

		req->hdr.type = RPM_REQ_MAGIC;
		req->hdr.len = len + REQ_MSG_LENGTH;
		req->req_hdr.id = ++msg_id;
		req->req_hdr.set = 0;
		req->req_hdr.resourceType = data[RESOURCETYPE];
		req->req_hdr.resourceId = data[RESOURCEID];
		req->req_hdr.dataLength = len;

		len_to_smd = req->req_hdr.dataLength + sizeof(*req) -
			2 * sizeof(uint32_t);

		memcpy(req->payload, &data[2], req->req_hdr.dataLength);

		ret = smd_write(&ch, (void *)req, len_to_smd, SMD_APPS_RPM);
		free(req);
		if (ret) {
			printf("Failed to send RPM request\n");
			return ret;
		}

		/* Read the response */
		ret = rpm_read_ack();
		smd_signal_read_complete(&ch, 0);
		break;

	case RPM_CMD_TYPE:
		cmd.hdr.type = RPM_CMD_MAGIC;
		cmd.hdr.len = CMD_MSG_LENGTH;
		len_to_smd = sizeof(rpm_cmd);

		fill_kvp_object(&cmd.data, data, len);
		ret = smd_write(&ch, (void *)&cmd, len_to_smd, SMD_APPS_RPM);
		free(cmd.data);
		if (ret)
			printf("Failed to send RPM request\n");
		break;

	default:
		printf("Invalid RPM request type: %02x\n", type);
		ret = -EINVAL;
	}

	return ret;
}

uint32_t rpm_recv_data(uint32_t *response, uint32_t *len)
{
	msg_type type;
	rpm_ack_msg *m;
	int ret = smd_read(&ch, len, SMD_APPS_RPM, response);

	if (ret)
		return 0;
	if (*len == 0) {
		printf("Failed to read data from RPM\n");
		return 0;
	}

	invalidate_dcache_range((unsigned long)response, *len);

	m = (void *)response;
	if (m->hdr.type == RPM_CMD_MAGIC) {
		type = RPM_CMD_TYPE;
	} else if(m->hdr.type == RPM_REQ_MAGIC) {
		type = RPM_REQUEST_TYPE;
	} else {
		printf("Invalid RPM Header type %02x\n",
			m->hdr.type);
		return 0;
	}

	if (type == RPM_CMD_TYPE && m->hdr.len == ACK_MSG_LENGTH) {
		debug("Received SUCCESS CMD ACK\n");
	} else if (type == RPM_REQUEST_TYPE && m->hdr.len == ACK_MSG_LENGTH) {
		debug("Received SUCCESS REQ ACK\n");
	} else {
		printf("Received ERROR ACK\n");
		return 0;
	}
	debug("Read %u bytes from SMD\n", *len);
	return *len;
}
