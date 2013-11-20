/*
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
 * based on: code from RedBoot (C) Uwe Steinkohl <US@KARO-electronics.de>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <net.h>
#include <wince.h>
#include <asm/errno.h>

DECLARE_GLOBAL_DATA_PTR;

static enum bootme_state bootme_state;
static int bootme_src_port = 0xdeadface;
static int bootme_dst_port = 0xdeadbeef;
static uchar bootme_ether[ETH_ALEN];
static IPaddr_t bootme_ip;
static int bootme_timed_out;
static const char *output_packet; /* used by first send udp */
static int output_packet_len;
static unsigned long bootme_timeout;
static bootme_hand_f *bootme_packet_handler;

#ifdef DEBUG
static void __attribute__((unused)) ce_dump_block(const void *ptr, int length)
{
	const char *p = ptr;
	int i;
	int j;

	for (i = 0; i < length; i++) {
		if (!(i % 16)) {
			printf("\n%p: ", ptr + i);
		}

		printf("%02x ", p[i]);
		if (!((i + 1) % 16)){
			printf("      ");
			for (j = i - 15; j <= i; j++){
				if((p[j] > 0x1f) && (p[j] < 0x7f)) {
					printf("%c", p[j]);
				} else {
					printf(".");
				}
			}
		}
	}
	printf("\n");
}
#else
static inline void ce_dump_block(void *ptr, int length)
{
}
#endif

static void bootme_timeout_handler(void)
{
	printf("%s\n", __func__);
	net_set_state(NETLOOP_SUCCESS);
	bootme_timed_out++;
}

static inline int env_changed(int *id)
{
	int env_id = get_env_id();

	if (*id != env_id) {
		debug("env_id: %d -> %d\n", *id, env_id);
		*id = env_id;
		return 1;
	}
	return 0;
}

static int env_id;

static int is_broadcast(IPaddr_t ip)
{
	static IPaddr_t netmask;
	static IPaddr_t our_ip;

	return (ip == ~0 ||				/* 255.255.255.255 */
	    ((netmask & our_ip) == (netmask & ip) &&	/* on the same net */
	    (netmask | ip) == ~0));		/* broadcast to our net */
}

static int check_net_config(void)
{
	if (env_changed(&env_id)) {
		char *p;
		char *bip;

		bootme_dst_port = EDBG_DOWNLOAD_PORT;
		if (bootme_ip == 0) {
			bip = getenv("bootmeip");
			if (bip) {
				bootme_ip = getenv_IPaddr("bootmeip");
				if (!bootme_ip)
					return -EINVAL;
				p = strchr(bip, ':');
				if (p) {
					bootme_dst_port = simple_strtoul(p + 1, NULL, 10);
				}
			} else {
				memset(&bootme_ip, 0xff, sizeof(bootme_ip));
			}
		}

		p = getenv("bootme_dst_port");
		if (p)
			bootme_dst_port = simple_strtoul(p, NULL, 10);

		p = getenv("bootme_src_port");
		if (p)
			bootme_src_port = simple_strtoul(p, NULL, 10);
		else
			bootme_src_port = bootme_dst_port;

		if (is_broadcast(bootme_ip))
			memset(bootme_ether, 0xff, sizeof(bootme_ether));
		else
			memset(bootme_ether, 0, sizeof(bootme_ether));

		net_init();
		NetServerIP = bootme_ip;
	}
	return 0;
}

static void bootme_wait_arp_handler(uchar *pkt, unsigned dest,
				IPaddr_t sip, unsigned src,
				unsigned len)
{
	net_set_state(NETLOOP_SUCCESS); /* got arp reply - quit net loop */
}

static inline char next_cursor(char c)
{
	switch(c) {
	case '-':
		return '\\';
	case '\\':
		return '|';
	case '|':
		return '/';
	case '/':
		return '-';
	}
	return 0;
}

static void bootme_handler(uchar *pkt, unsigned dest_port, IPaddr_t src_ip,
			unsigned src_port, unsigned len)
{
	uchar *eth_pkt = pkt;
	unsigned eth_len = len;
	static char cursor = '|';
	enum bootme_state last_state = bootme_state;

	debug("received packet of len %d from %pI4:%d to port %d\n",
		len, &src_ip, src_port, dest_port);
	ce_dump_block(pkt, len);

	if (!bootme_packet_handler) {
		printf("No packet handler set for BOOTME protocol; dropping packet\n");
		return;
	}
	if (dest_port != bootme_src_port || !len)
		return; /* not for us */

	printf("%c\x08", cursor);
	cursor = next_cursor(cursor);

	if (!is_broadcast(bootme_ip) && src_ip != bootme_ip) {
		debug("src_ip %pI4 does not match destination IP %pI4\n",
			&src_ip, &bootme_ip);
		return; /* not from our server */
	}
	if (bootme_state == BOOTME_INIT || bootme_state == BOOTME_DEBUG_INIT) {
		struct ethernet_hdr *eth = (struct ethernet_hdr *)(pkt -
					NetEthHdrSize() - IP_UDP_HDR_SIZE);
		memcpy(bootme_ether, eth->et_src, sizeof(bootme_ether));
		printf("Target MAC address set to %pM\n", bootme_ether);

		if (is_broadcast(bootme_ip)) {
			NetCopyIP(&bootme_ip, &src_ip);
		}
	}
	if (bootme_state == BOOTME_INIT) {
		bootme_src_port = EDBG_SVC_PORT;
		debug("%s: bootme_src_port set to %d\n", __func__, bootme_src_port);
	}

	debug("bootme_dst_port %d -> %d\n", bootme_dst_port, src_port);
	bootme_dst_port = src_port;

	bootme_state = bootme_packet_handler(eth_pkt, eth_len);
	debug("bootme_packet_handler() returned %d\n", bootme_state);
	if (bootme_state != last_state)
		debug("%s@%d: bootme_state: %d -> %d\n", __func__, __LINE__,
			last_state, bootme_state);
	switch (bootme_state) {
	case BOOTME_INIT:
	case BOOTME_DEBUG_INIT:
		break;

	case BOOTME_DOWNLOAD:
		if (last_state != BOOTME_INIT)
			NetBootFileXferSize += len - 4;
		/* fallthru */
	case BOOTME_DEBUG:
		if (last_state == BOOTME_INIT ||
			last_state == BOOTME_DEBUG_INIT)
			bootme_timeout = 3 * 1000;
		NetSetTimeout(bootme_timeout, bootme_timeout_handler);
		break;

	case BOOTME_DONE:
		net_set_state(NETLOOP_SUCCESS);
		bootme_packet_handler = NULL;
		break;

	case BOOTME_ERROR:
		net_set_state(NETLOOP_FAIL);
		bootme_packet_handler = NULL;
	}
}

void BootmeStart(void)
{
	if (bootme_state != BOOTME_DOWNLOAD)
		check_net_config();

	if (output_packet_len == 0 ||
		is_valid_ether_addr(bootme_ether)) {
		/* wait for incoming packet */
		net_set_udp_handler(bootme_handler);
		bootme_timed_out = 0;
		NetSetTimeout(bootme_timeout, bootme_timeout_handler);
	} else {
		/* send ARP request */
		uchar *pkt;

		net_set_arp_handler(bootme_wait_arp_handler);
		assert(NetTxPacket != NULL);
		pkt = (uchar *)NetTxPacket + NetEthHdrSize() + IP_UDP_HDR_SIZE;
		memcpy(pkt, output_packet, output_packet_len);
		debug("%s@%d: Sending ARP request:\n", __func__, __LINE__);
		ce_dump_block(pkt, output_packet_len);
		NetSendUDPPacket(bootme_ether, bootme_ip, bootme_dst_port,
				bootme_src_port, output_packet_len);
		output_packet_len = 0;
	}
}

int bootme_send_frame(const void *buf, size_t len)
{
	int ret;
	struct eth_device *eth;
	uchar *pkt;

	eth = eth_get_dev();
	if (eth == NULL)
		return -EINVAL;

	if (bootme_state == BOOTME_INIT || bootme_state == BOOTME_DEBUG_INIT)
		check_net_config();

	debug("%s: buf: %p len: %u from %pI4:%d to %pI4:%d\n",
		__func__, buf, len, &NetOurIP, bootme_src_port, &bootme_ip,
		bootme_dst_port);

	if (is_zero_ether_addr(bootme_ether)) {
		output_packet = buf;
		output_packet_len = len;
		/* wait for arp reply and send packet */
		ret = NetLoop(BOOTME);
		if (ret < 0) {
			/* drop packet */
			output_packet_len = 0;
			return ret;
		}
		if (bootme_timed_out)
			return -ETIMEDOUT;
		return 0;
	}

	if (eth->state != ETH_STATE_ACTIVE) {
		if (eth_is_on_demand_init()) {
			ret = eth_init(gd->bd);
			if (ret < 0)
				return ret;
			eth_set_last_protocol(BOOTME);
		} else {
			eth_init_state_only(gd->bd);
		}
	}

	assert(NetTxPacket != NULL);
	pkt = (uchar *)NetTxPacket + NetEthHdrSize() + IP_UDP_HDR_SIZE;
	memcpy(pkt, buf, len);

	ret = NetSendUDPPacket(bootme_ether, bootme_ip, bootme_dst_port,
			bootme_src_port, len);
	if (ret)
		printf("Failed to send packet: %d\n", ret);

	return ret;
}

static void bootme_init(IPaddr_t server_ip)
{
	debug("%s@%d: bootme_state: %d -> %d\n", __func__, __LINE__,
		bootme_state, BOOTME_INIT);
	bootme_state = BOOTME_INIT;
	bootme_ip = server_ip;
	/* force reconfiguration in check_net_config() */
	env_id = 0;
}

int BootMeDownload(bootme_hand_f *handler)
{
	int ret;

	bootme_packet_handler = handler;

	ret = NetLoop(BOOTME);
	if (ret < 0)
		return BOOTME_ERROR;
	if (bootme_timed_out && bootme_state != BOOTME_INIT)
		return BOOTME_ERROR;

	return bootme_state;
}

int BootMeDebugStart(bootme_hand_f *handler)
{
	int ret;

	bootme_packet_handler = handler;

	bootme_init(bootme_ip);
	debug("%s@%d: bootme_state: %d -> %d\n", __func__, __LINE__,
		bootme_state, BOOTME_DEBUG_INIT);
	bootme_state = BOOTME_DEBUG_INIT;

	bootme_timeout = 3 * 1000;
	NetSetTimeout(bootme_timeout, bootme_timeout_handler);

	ret = NetLoop(BOOTME);
	if (ret < 0)
		return BOOTME_ERROR;
	if (bootme_timed_out)
		return BOOTME_DONE;
	return bootme_state;
}

int BootMeRequest(IPaddr_t server_ip, const void *buf, size_t len, int timeout)
{
	bootme_init(server_ip);
	bootme_timeout = timeout * 1000;
	bootme_timed_out = 0;
	NetSetTimeout(bootme_timeout, bootme_timeout_handler);
	return bootme_send_frame(buf, len);
}
