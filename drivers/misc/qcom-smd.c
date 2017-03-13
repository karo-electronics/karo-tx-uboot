/*
 * Qualcomm Shared Memory driver
 *
 * Copyright (C) 2017, Lothar Waßmann <LW@KARO-electronics.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * based on: platform/msm_shared/smd.c
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
#include <console.h>
#include <errno.h>
#include <malloc.h>
#include <asm/io.h>
#include <mach/qcom-smd.h>
#include <power/pmic-qcom-smd-rpm.h>

#include "qcom-smem.h"

#define SMD_CHANNEL_ACCESS_RETRY	1000000
#define APCS_ALIAS0_IPC_INTERRUPT	((void *)GICD_BASE + 0x11008)

static smd_channel_alloc_entry_t *smd_channel_alloc_entry;

static void smd_notify_rpm(smd_channel_info_t *ch);

#ifdef DEBUG
static inline void __smd_dump_state(smd_channel_info_t *ch)
{
	printf("   st DSR CTS CD RI dw dr su ma ri wi\n");
	printf("TX@%p:\n", &ch->port_info->ch0);
	printf("%u ", ch->port_info->ch0.stream_state);
	printf(" %2u ", ch->port_info->ch0.DTR_DSR);
	printf(" %2u ", ch->port_info->ch0.CTS_RTS);
	printf("%2u ", ch->port_info->ch0.CD);
	printf("%2u ", ch->port_info->ch0.RI);
	printf("%2u ", ch->port_info->ch0.data_written);
	printf("%2u ", ch->port_info->ch0.data_read);
	printf("%2u ", ch->port_info->ch0.state_updated);
	printf("%2u ", ch->port_info->ch0.mask_recv_intr);
	printf("%2u ", ch->port_info->ch0.read_index);
	printf("%2u ", ch->port_info->ch0.write_index);
	printf("\n");

	printf("RX@%p:\n", &ch->port_info->ch1);
	printf("%u ", ch->port_info->ch1.stream_state);
	printf(" %2u ", ch->port_info->ch1.DTR_DSR);
	printf(" %2u ", ch->port_info->ch1.CTS_RTS);
	printf("%2u ", ch->port_info->ch1.CD);
	printf("%2u ", ch->port_info->ch1.RI);
	printf("%2u ", ch->port_info->ch1.data_written);
	printf("%2u ", ch->port_info->ch1.data_read);
	printf("%2u ", ch->port_info->ch1.state_updated);
	printf("%2u ", ch->port_info->ch1.mask_recv_intr);
	printf("%2u ", ch->port_info->ch1.read_index);
	printf("%2u ", ch->port_info->ch1.write_index);
	printf("\n");
}
#define smd_dump_state(ch) __smd_dump_state(ch)

static inline void __smd_set_var(uint32_t *var, int val,
			const char *ch, const char *elem,
			const char *fn, int ln)
{
	if (*var != val) {
		debug("%s@%d: setting %s.%s %08x -> %08x\n", fn, ln,
			ch, elem, *var, val);
		*var = val;
	} else {
		debug("%s@%d: %s.%s unchanged: %08x\n", fn, ln,
			ch, elem, *var);
	}
}
#define smd_set_var(chan, var, val)	__smd_set_var(&ch->port_info->chan.var, val, \
						#chan, #var, __func__, __LINE__)
#else
static inline void smd_dump_state(smd_channel_info_t *ch)
{
}

#define smd_set_var(chan, var, val)	ch->port_info->chan.var = val
#endif

static void smd_write_state(smd_channel_info_t *ch, uint32_t state)
{
	if (state == SMD_SS_OPENED) {
		smd_set_var(ch0, DTR_DSR, 1);
		smd_set_var(ch0, CTS_RTS, 1);
		smd_set_var(ch0, CD, 1);
	} else {
		smd_set_var(ch0, DTR_DSR, 0);
		smd_set_var(ch0, CTS_RTS, 0);
		smd_set_var(ch0, CD, 0);
	}

	smd_set_var(ch0, stream_state, state);
}

static void smd_state_update(smd_channel_info_t *ch, uint32_t flag)
{
	if (flag)
		smd_set_var(ch0, state_updated, flag);
	smd_dump_state(ch);
	flush_dcache_range((unsigned long)ch->port_info, sizeof(*ch));
}

static void smd_set_state(smd_channel_info_t *ch, uint32_t state, uint32_t flag)
{
	uint32_t current_state;
	uint32_t size;

	if (!ch->port_info) {
		ch->port_info = smem_get_alloc_entry(SMEM_SMD_BASE_ID +
						ch->alloc_entry.cid,
						&size);
		if (!ch->port_info) {
			printf("Failed to get sram entry for CID: %u\n",
				ch->alloc_entry.cid);
			return;
		}
	}

	current_state = ch->port_info->ch0.stream_state;
	debug("%s@%d: state: %u -> %u\n", __func__, __LINE__,
		current_state, state);

	switch (state) {
	case SMD_SS_CLOSED:
		if (current_state == SMD_SS_OPENED) {
			smd_write_state(ch, SMD_SS_CLOSING);
		} else {
			smd_write_state(ch, SMD_SS_CLOSED);
		}
		break;
	case SMD_SS_OPENING:
		if (current_state == SMD_SS_CLOSING || current_state == SMD_SS_CLOSED) {
			smd_write_state(ch, SMD_SS_OPENING);
			smd_set_var(ch1, read_index, 0);
			smd_set_var(ch0, write_index, 0);
			smd_set_var(ch0, mask_recv_intr, 0);
		}
		break;
	case SMD_SS_OPENED:
		if (current_state == SMD_SS_OPENING) {
			smd_write_state(ch, SMD_SS_OPENED);
		}
		break;
	case SMD_SS_CLOSING:
		if (current_state == SMD_SS_OPENED) {
			smd_write_state(ch, SMD_SS_CLOSING);
		}
		break;
	case SMD_SS_FLUSHING:
	case SMD_SS_RESET:
	case SMD_SS_RESET_OPENING:
	default:
		break;
	}

	ch->current_state = state;

	smd_state_update(ch, flag);
}

#define SMD_TIMEOUT_MS 10

static void smd_irq_handler(smd_channel_info_t *ch)
{
	static int closed;

	smd_dump_state(ch);
	if (ch->current_state == SMD_SS_CLOSED) {
		debug("%s@%d: remote state=%u\n", __func__, __LINE__,
			ch->port_info->ch1.stream_state);
		if (!closed) {
			closed = 1;
			smd_set_var(ch0, mask_recv_intr, 1);
			smd_set_var(ch0, data_written, 0);
			smd_set_var(ch0, write_index, 0);
			smd_set_var(ch1, read_index, 0);
			smd_set_var(ch0, read_index, 0);
			smd_set_state(ch, SMD_SS_CLOSED, 1);
			smd_notify_rpm(ch);
		} else {
			smd_set_var(ch1, stream_state, SMD_SS_OPENING);
			smd_set_var(ch1, read_index, 0);
			smd_set_var(ch1, write_index, 0);
			free(smd_channel_alloc_entry);
			smd_channel_alloc_entry = NULL;
		}
		return;
	} else {
		closed = 0;
	}

	debug("%s@%d: ch0: updated: %d state %u -> %u\n", __func__, __LINE__,
		ch->port_info->ch0.state_updated,
		ch->current_state, ch->port_info->ch0.stream_state);
	debug("%s@%d: ch1: updated: %d state %u -> %u\n", __func__, __LINE__,
		ch->port_info->ch1.state_updated,
		ch->current_state, ch->port_info->ch1.stream_state);

	if (ch->port_info->ch1.state_updated)
		smd_set_var(ch1, state_updated, 0);

	/* Should we have to use a do while and change states until we complete */
	while (ch->current_state != ch->port_info->ch1.stream_state)
		smd_set_state(ch, ch->port_info->ch1.stream_state, 0);

	if (ch->current_state == SMD_SS_CLOSING) {
		smd_set_state(ch, SMD_SS_CLOSED, 1);
		smd_notify_rpm(ch);
		udelay(1);
		debug("SMD channel released\n");
	}
}

static int __smd_check_irq(smd_channel_info_t *ch,
			const char *fn, int ln)
{
	int ret;
	unsigned long irqpend;
	unsigned long irqactive;

	irqpend = readl(0x0b000298);
	irqactive = readl(0x0b000318);
	ret = ((irqpend | irqactive) & 0x100) != 0;
	if (ret) {
		debug("%s@%d: GIC IRQPEND: %08lx IRQACTIVE: %08lx\n",
			fn, ln, irqpend, irqactive);
		smd_irq_handler(ch);
		writel(0x100, 0x0b000298);
	}
	return ret;
}
#define smd_check_irq(ch)	__smd_check_irq(ch, __func__, __LINE__)

static inline int wait_for_smd_irq(smd_channel_info_t *ch)
{
	int ret;
	int first = 1;
	unsigned long start = get_timer(0);
	unsigned long elapsed = 0;

	do {
		ret = smd_check_irq(ch);
		if (ret)
			break;
		if (first) {
			debug("Waiting for SMD interrupt\n");
			first = 0;
		}
		if ((elapsed = get_timer(start)) > SMD_TIMEOUT_MS) {
			printf("Wait for SMD completion timed out\n");
			break;
		}
		udelay(1);
	} while (1);
	if (ret || smd_check_irq(ch)) {
		debug("GIC IRQ arrived after %lu ticks\n", elapsed);
		return 1;
	}
	return 0;
}

static void smd_notify_rpm(smd_channel_info_t *ch)
{
	smd_check_irq(ch);
	/* Set BIT 0 to notify RPM via IPC interrupt*/
	writel(BIT(0), APCS_ALIAS0_IPC_INTERRUPT);
	smd_check_irq(ch);
}

static int smd_get_channel_entry(smd_channel_info_t *ch, uint32_t ch_type)
{
	int i;

	for (i = 0; i < SMEM_NUM_SMD_STREAM_CHANNELS; i++) {
		if ((smd_channel_alloc_entry[i].ctype & 0xFF) == ch_type) {
			memcpy(&ch->alloc_entry, &smd_channel_alloc_entry[i],
				sizeof(smd_channel_alloc_entry_t));
			return 1;
		}
	}
	/* Channel not found, retry again */
	printf("Channel not found, wait and retry for the update\n");
	return 0;
}

static int smd_get_channel_info(smd_channel_info_t *ch, uint32_t ch_type)
{
	int ret;
	uint8_t *fifo_buf;
	uint32_t fifo_buf_size;
	uint32_t size;

	ret = smd_get_channel_entry(ch, ch_type);
	if (!ret)
		return ret;

	ch->port_info = smem_get_alloc_entry(SMEM_SMD_BASE_ID + ch->alloc_entry.cid,
					&size);
	if (ch->port_info == NULL) {
		printf("Failed to get smem entry type %u\n",
			SMEM_SMD_BASE_ID + ch->alloc_entry.cid);
		return 0;
	}

	fifo_buf = smem_get_alloc_entry(SMEM_SMD_FIFO_BASE_ID + ch->alloc_entry.cid,
					&fifo_buf_size);

	fifo_buf_size /= 2;
	ch->send_buf = fifo_buf;
	ch->recv_buf = fifo_buf + fifo_buf_size;
	ch->fifo_size = fifo_buf_size;

	return 1;
}

static bool is_channel_open(smd_channel_info_t *ch)
{
	if (ch->port_info->ch0.stream_state == SMD_SS_OPENED &&
		(ch->port_info->ch1.stream_state == SMD_SS_OPENED ||
			ch->port_info->ch1.stream_state == SMD_SS_FLUSHING))
		return true;
	else
		return false;
}

static inline uint32_t *smd_fifo_entry(void *fifo, int index)
{
	return (uint32_t *)(fifo + index);
}

/* Copy the local buffer to fifo buffer.
 * Takes care of fifo overlap.
 * Uses the fifo as circular buffer, if the request data
 * exceeds the max size of the buffer start from the beginning.
 */
static void memcpy_to_fifo(smd_channel_info_t *ch_ptr, const uint32_t *src,
			size_t len)
{
	uint32_t write_index = ch_ptr->port_info->ch0.write_index;
	uint32_t *dest = smd_fifo_entry(ch_ptr->send_buf, write_index);

	while (len) {
		*dest++ = *src++;
		len -= 4;
		write_index += 4;
		if (write_index >= ch_ptr->fifo_size) {
			write_index = 0;
			dest = smd_fifo_entry(ch_ptr->send_buf, write_index);
		}
	}
	ch_ptr->port_info->ch0.write_index = write_index;
}

/* Copy the fifo buffer to a local destination.
 * Takes care of fifo overlap.
 * If the response data is split across with some part at
 * end of fifo and some at the beginning of the fifo
 */
static void memcpy_from_fifo(smd_channel_info_t *ch_ptr, uint32_t *dest,
			size_t len)
{
	uint32_t read_index = ch_ptr->port_info->ch1.read_index;
	uint32_t *src = smd_fifo_entry(ch_ptr->recv_buf, read_index);

	while (len) {
		if (dest)
			*dest++ = *src++;
		else
			printf("Discarding %08x read from FIFO\n", *src++);
		len -= 4;
		read_index += 4;
		if (read_index >= ch_ptr->fifo_size) {
			read_index = 0;
			src = smd_fifo_entry(ch_ptr->recv_buf, read_index);
		}
	}
	ch_ptr->port_info->ch1.read_index = read_index;
}

struct rpm_message {
	rpm_gen_hdr hdr;
	union {
		u8 message;
		uint32_t val;
	};
};

struct rpm_message_hdr {
	rpm_gen_hdr hdr;
	struct rpm_message msg;
};

void smd_parse_response(const void *buf, size_t size)
{
	const struct rpm_message_hdr *hdr = buf;
	const struct rpm_message *msg = &hdr->msg;
	size -= sizeof(rpm_gen_hdr);

	switch (le32_to_cpu(hdr->hdr.type)) {
	case 0x00716572:
		while (size) {
			size_t len = ALIGN(msg->hdr.len, sizeof(uint32_t)) +
				sizeof(rpm_gen_hdr);

			if (len > size) {
				printf("Invalid RPM MSG length %zu received\n",
					len);
				return;
			}

			switch (msg->hdr.type) {
			case 0x2367736d:
				break;
			case 0x00727265:
				printf("SMD ERR: %*s\n",
					msg->hdr.len, &msg->message);
				break;
			default:
				printf("Unhandled RPM MSG type 0x%02x\n",
					msg->hdr.type);
			}
			size -= len;
			msg = (void *)msg + len;
		}
		break;
	default:
		printf("Unsupported MSG type: %02x\n", hdr->hdr.type);
	}
}

static inline int smd_read_avail(smd_channel_info_t *ch, uint32_t size)
{
	smd_shared_stream_info_type *ch1 = &ch->port_info->ch1;

	invalidate_dcache_range((unsigned long)ch->port_info, size);
	return ch1->write_index - ch1->read_index;
}

int smd_read(smd_channel_info_t *ch, uint32_t *len, int ch_type,
	uint32_t *response)
{
	smd_pkt_hdr smd_hdr = {};
	uint32_t size;
	int timeout = 10000;

	/* Read the indices from smem */
	ch->port_info = smem_get_alloc_entry(SMEM_SMD_BASE_ID + ch->alloc_entry.cid,
					&size);
	if (!ch->port_info->ch1.DTR_DSR) {
		printf("%s: DTR is off\n", __func__);
		return -EBUSY;
	}

	/* Wait until the data size in the smd buffer is >= smd packet header size */
	while (smd_read_avail(ch, size) < sizeof(smd_pkt_hdr)) {
		if (--timeout < 0)
			return -ETIMEDOUT;
		udelay(100);
	}

	/* Copy the smd buffer to local buf */
	memcpy_from_fifo(ch, (uint32_t *)&smd_hdr, sizeof(smd_hdr));

	invalidate_dcache_range((unsigned long)&smd_hdr, sizeof(smd_hdr));

	/* Wait on the data being updated in SMEM before returning the response */
	while ((ch->port_info->ch1.write_index - ch->port_info->ch1.read_index) <
		smd_hdr.pkt_size) {
		/* Get the update info from memory */
		invalidate_dcache_range((unsigned long)ch->port_info, size);
	}

	/* We are good to return the response now */
	if (*len < smd_hdr.pkt_size) {
		size_t size = smd_hdr.pkt_size;
		static char *msgbuf;
		static size_t buflen;
		void *buf;

		debug("Input buffer too small (%u bytes) for response (%zu bytes)\n",
			*len, size);

		*len = 0;
		if (!msgbuf || buflen < size) {
			free(msgbuf);
			msgbuf = malloc(size);
			if (!msgbuf) {
				printf("Failed to allocate %zu bytes for SMD response buffer\n",
					size);
				return -ENOMEM;
			}
			buflen = size;
		}
		buf = msgbuf;
		memcpy_from_fifo(ch, buf, size);
		smd_parse_response(buf, size);
	} else {
		memcpy_from_fifo(ch, response, smd_hdr.pkt_size);
		*len = smd_hdr.pkt_size;
	}
	invalidate_dcache_range((unsigned long)response, *len);
	debug("Read %u bytes from SMD\n", smd_hdr.pkt_size);
	return 0;
}

void smd_signal_read_complete(smd_channel_info_t *ch, uint32_t len)
{
	/* Clear the data_written flag */
	smd_set_var(ch1, data_written, 0);

	/* Set the data_read flag */
	smd_set_var(ch0, data_read, 1);

	if (!ch->port_info->ch1.mask_recv_intr) {
		dsb();
		smd_notify_rpm(ch);
	}
}

int smd_write(smd_channel_info_t *ch, const void *data, uint32_t len, int ch_type)
{
	smd_pkt_hdr smd_hdr = {};
	uint32_t size;

	if (len + sizeof(smd_hdr) > ch->fifo_size) {
		printf("%s: len %lu is greater than fifo size (%u)\n",
			__func__, len + sizeof(smd_hdr), ch->fifo_size);
		return -EMSGSIZE;
	}

	/* Read the indices from smem */
	ch->port_info = smem_get_alloc_entry(SMEM_SMD_BASE_ID + ch->alloc_entry.cid,
					&size);

	if (!is_channel_open(ch)) {
		printf("%s: channel is not in OPEN state \n", __func__);
		return -EINVAL;
	}

	if (!ch->port_info->ch0.DTR_DSR) {
		printf("%s: DTR is off\n", __func__);
		return -EBUSY;
	}

	/* Clear the data_read flag */
	smd_set_var(ch1, data_read, 0);

	/* copy the local buf to smd buf */
	smd_hdr.pkt_size = len;

	memcpy_to_fifo(ch, (uint32_t *)&smd_hdr, sizeof(smd_hdr));

	memcpy_to_fifo(ch, data, len);

	dsb();

	/* Set the necessary flags */

	smd_set_var(ch0, data_written, 1);
	smd_set_var(ch0, mask_recv_intr, 0);

	dsb();

	smd_notify_rpm(ch);

	return 0;
}

int smd_init(smd_channel_info_t *ch, uint32_t ch_type)
{
	int ret;
	int chnl_found;
	int timeout = SMD_CHANNEL_ACCESS_RETRY;

	writel(0x100, 0x0b000298);
	smd_channel_alloc_entry = memalign(CONFIG_SYS_CACHELINE_SIZE,
					SMD_CHANNEL_ALLOC_MAX);
	if (smd_channel_alloc_entry == NULL) {
		printf("Failed to allocate %u bytes for SMD channels\n",
			SMD_CHANNEL_ALLOC_MAX);
		return -ENOMEM;
	}

	printf("Waiting for the RPM to populate smd channel table\n");

	do {
		ret = smem_read_alloc_entry(SMEM_CHANNEL_ALLOC_TBL,
					smd_channel_alloc_entry,
					SMD_CHANNEL_ALLOC_MAX);
		if (ret) {
			printf("ERROR reading smem channel alloc tbl\n");
			return ret;
		}

		chnl_found = smd_get_channel_info(ch, ch_type);
		timeout--;
		udelay(10);
	} while (timeout > 0 && !chnl_found);

	if (timeout <= 0) {
		printf("Apps timed out waiting for RPM-->APPS channel entry\n");
		return -ETIMEDOUT;
	}

	smd_dump_state(ch);
	smd_set_state(ch, SMD_SS_OPENING, 1);

	smd_notify_rpm(ch);
	wait_for_smd_irq(ch);
	return 0;
}

void smd_uninit(smd_channel_info_t *ch)
{
	if (ch->current_state == SMD_SS_CLOSED)
		return;

	smd_set_state(ch, SMD_SS_CLOSING, 1);
	smd_notify_rpm(ch);

	while (ch->current_state != SMD_SS_CLOSED) {
		wait_for_smd_irq(ch);
		if (ctrlc())
			break;
	}

	if (!had_ctrlc()) {
		/* wait until smem entry has been deallocated in smd_irq_handler() */
		while (smd_channel_alloc_entry) {
			wait_for_smd_irq(ch);
			if (ctrlc())
				break;
		}
	}

	smd_notify_rpm(ch);

	if (had_ctrlc())
		clear_ctrlc();
}
