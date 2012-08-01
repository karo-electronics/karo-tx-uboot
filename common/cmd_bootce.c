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

#define _debug(fmt...) do {} while (0)

DECLARE_GLOBAL_DATA_PTR;

#define WINCE_VRAM_BASE		0x80000000
#define CE_FIX_ADDRESS(a)	((void *)((a) - WINCE_VRAM_BASE + CONFIG_SYS_SDRAM_BASE))

#ifndef INT_MAX
#define INT_MAX			((1U << (sizeof(int) * 8 - 1)) - 1)
#endif

/* Bin image parse states */
#define CE_PS_RTI_ADDR		0
#define CE_PS_RTI_LEN		1
#define CE_PS_E_ADDR		2
#define CE_PS_E_LEN		3
#define CE_PS_E_CHKSUM		4
#define CE_PS_E_DATA		5

#define CE_MIN(a, b)		(((a) < (b)) ? (a) : (b))
#define CE_MAX(a, b)		(((a) > (b)) ? (a) : (b))

#define _STRMAC(s)		#s
#define STRMAC(s)		_STRMAC(s)

static ce_bin __attribute__ ((aligned (32))) g_bin;
static ce_net __attribute__ ((aligned (32))) g_net;

static inline void print_IPaddr(IPaddr_t ip)
{
	printf("%d.%d.%d.%d",
		ip & 0xff,
		(ip >> 8) & 0xff,
		(ip >> 16) & 0xff,
		(ip >> 24) & 0xff);
}

static void ce_init_bin(ce_bin *bin, unsigned char *dataBuffer)
{
	memset(bin, 0, sizeof(*bin));

	bin->data = dataBuffer;
	bin->parseState = CE_PS_RTI_ADDR;
	bin->parsePtr = (unsigned char *)bin;
	debug("parsePtr=%p\n", bin->parsePtr);
}

static int ce_is_bin_image(void *image, int imglen)
{
	if (imglen < CE_BIN_SIGN_LEN) {
		return 0;
	}

	return memcmp(image, CE_BIN_SIGN, CE_BIN_SIGN_LEN) == 0;
}

static const struct ce_magic {
	char magic[8];
	size_t size;
	ce_std_driver_globals drv_glb;
} ce_magic_template = {
	.magic = "KARO_CE6",
	.size = sizeof(ce_std_driver_globals),
	.drv_glb = {
		.header = {
			.signature = STD_DRV_GLB_SIGNATURE,
			.oalVersion = 1,
			.bspVersion = 1,
		},
	},
};

#ifdef DEBUG
static void __attribute__((unused)) ce_dump_block(void *ptr, int length)
{
	char *p = ptr;
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

static void ce_setup_std_drv_globals(ce_std_driver_globals *std_drv_glb,
				ce_bin *bin)
{
	if (eth_get_dev()) {
		debug("Copying MAC from %p to %p..%p\n",
			eth_get_dev()->enetaddr, &std_drv_glb->kitl.mac,
			(void *)&std_drv_glb->kitl.mac + sizeof(std_drv_glb->kitl.mac) - 1);
		memcpy(&std_drv_glb->kitl.mac, eth_get_dev()->enetaddr,
			sizeof(std_drv_glb->kitl.mac));
	}
	snprintf(std_drv_glb->deviceId, sizeof(std_drv_glb->deviceId),
		"Triton%02X", std_drv_glb->kitl.mac[2] & 0xff);

	std_drv_glb->kitl.ipAddress = gd->bd->bi_ip_addr;
	std_drv_glb->kitl.ipMask = getenv_IPaddr("netmask");
	std_drv_glb->kitl.ipRoute = getenv_IPaddr("gatewayip");
	std_drv_glb->mtdparts = getenv("mtdparts");
}

static void ce_prepare_run_bin(ce_bin *bin)
{
	ce_driver_globals *drv_glb;
	struct ce_magic *ce_magic = (void *)CONFIG_SYS_SDRAM_BASE + 0x160;
	ce_std_driver_globals *std_drv_glb = &ce_magic->drv_glb;

	/* Clear os RAM area (if needed) */
	if (bin->edbgConfig.flags & EDBG_FL_CLEANBOOT) {
		debug("cleaning memory from %p to %p\n",
			bin->eRamStart, bin->eRamStart + bin->eRamLen);

		printf("Preparing clean boot ... ");
		memset(bin->eRamStart, 0, bin->eRamLen);
		printf("ok\n");
	}

	/* Prepare driver globals (if needed) */
	if (bin->eDrvGlb) {
		debug("Copying CE MAGIC from %p to %p..%p\n",
			&ce_magic_template, ce_magic, (void *)ce_magic + sizeof(*ce_magic));
		memcpy(ce_magic, &ce_magic_template, sizeof(*ce_magic));

		ce_setup_std_drv_globals(std_drv_glb, bin);
		ce_dump_block(ce_magic, sizeof(*ce_magic));

		drv_glb = bin->eDrvGlb;
		memset(drv_glb, 0, sizeof(*drv_glb));

		drv_glb->signature = DRV_GLB_SIGNATURE;

		/* Local ethernet MAC address */
		memcpy(drv_glb->macAddr, std_drv_glb->kitl.mac,
			sizeof(drv_glb->macAddr));
		debug("got MAC address %02x:%02x:%02x:%02x:%02x:%02x from environment\n",
			drv_glb->macAddr[0], drv_glb->macAddr[1],
			drv_glb->macAddr[2], drv_glb->macAddr[3],
			drv_glb->macAddr[4], drv_glb->macAddr[5]);

		/* Local IP address */
		drv_glb->ipAddr = getenv_IPaddr("ipaddr");

		/* Subnet mask */
		drv_glb->ipMask = getenv_IPaddr("netmask");

		/* Gateway config */
		drv_glb->ipGate = getenv_IPaddr("gatewayip");
#ifdef DEBUG
		debug("got IP address ");
		print_IPaddr(drv_glb->ipAddr);
		debug(" from environment\n");
		debug("got IP mask ");
		print_IPaddr(drv_glb->ipMask);
		debug(" from environment\n");
		debug("got gateway address ");
		print_IPaddr(drv_glb->ipGate);
		debug(" from environment\n");
#endif
		/* EDBG services config */
		memcpy(&drv_glb->edbgConfig, &bin->edbgConfig, sizeof(bin->edbgConfig));
	}

	/*
	 * Make sure, all the above makes it into SDRAM because
	 * WinCE switches the cache & MMU off, obviously without
	 * flushing it first!
	 */
	flush_dcache_all();
}

static int ce_lookup_ep_bin(ce_bin *bin)
{
	ce_rom_hdr *header;
	ce_toc_entry *tentry;
	e32_rom *e32;
	unsigned int i;
	uint32_t *sig = (uint32_t *)(bin->rtiPhysAddr + ROM_SIGNATURE_OFFSET);

	debug("Looking for TOC signature at %p\n", sig);

	/* Check image Table Of Contents (TOC) signature */
	if (*sig != ROM_SIGNATURE) {
		printf("Error: Did not find image TOC signature!\n");
		printf("Expected %08x at address %p; found %08x instead\n",
			ROM_SIGNATURE, sig, *sig);
		return 0;
	}

	/* Lookup entry point */
	header = CE_FIX_ADDRESS(*(unsigned int *)(bin->rtiPhysAddr +
						ROM_SIGNATURE_OFFSET +
						sizeof(unsigned int)));
	tentry = (ce_toc_entry *)(header + 1);

	for (i = 0; i < header->nummods; i++) {
		// Look for 'nk.exe' module
		if (strcmp(CE_FIX_ADDRESS(tentry[i].fileName), "nk.exe") == 0) {
			// Save entry point and RAM addresses

			e32 = CE_FIX_ADDRESS(tentry[i].e32Offset);

			bin->eEntryPoint = CE_FIX_ADDRESS(tentry[i].loadOffset) +
				e32->e32_entryrva;
			bin->eRamStart = CE_FIX_ADDRESS(header->ramStart);
			bin->eRamLen = header->ramEnd - header->ramStart;
			// Save driver_globals address
			// Must follow RAM section in CE config.bib file
			//
			// eg.
			//
			// RAM		80900000	03200000	RAM
			// DRV_GLB	83B00000	00001000	RESERVED
			//
			bin->eDrvGlb = CE_FIX_ADDRESS(header->ramEnd);
			return 1;
		}
	}

	// Error: Did not find 'nk.exe' module
	return 0;
}

static int ce_parse_bin(ce_bin *bin)
{
	unsigned char *pbData = bin->data;
	int len = bin->dataLen;
	int copyLen;

	debug("starting ce image parsing:\n\tbin->binLen: 0x%08X\n", bin->binLen);
	debug("\tpbData: %p..%p len: %u\n", pbData, pbData + len - 1, len);

	if (len) {
		if (bin->binLen == 0) {
			// Check for the .BIN signature first
			if (!ce_is_bin_image(pbData, len)) {
				printf("Error: Invalid or corrupted .BIN image!\n");
				return CE_PR_ERROR;
			}

			printf("Loading Windows CE .BIN image ...\n");
			// Skip signature
			len -= CE_BIN_SIGN_LEN;
			pbData += CE_BIN_SIGN_LEN;
		}

		while (len) {
			switch (bin->parseState) {
			case CE_PS_RTI_ADDR:
			case CE_PS_RTI_LEN:
			case CE_PS_E_ADDR:
			case CE_PS_E_LEN:
			case CE_PS_E_CHKSUM:
				copyLen = CE_MIN(sizeof(unsigned int) - bin->parseLen, len);
				memcpy(&bin->parsePtr[bin->parseLen], pbData, copyLen);

				bin->parseLen += copyLen;
				len -= copyLen;
				pbData += copyLen;

				if (bin->parseLen == sizeof(unsigned int)) {
					if (bin->parseState == CE_PS_RTI_ADDR) {
						debug("rtiPhysAddr: %p -> %p\n",
							bin->rtiPhysAddr,
							CE_FIX_ADDRESS(bin->rtiPhysAddr));
						bin->rtiPhysAddr = CE_FIX_ADDRESS(bin->rtiPhysAddr);
					} else if (bin->parseState == CE_PS_E_ADDR) {
						if (bin->ePhysAddr) {
							_debug("ePhysAddr: %p -> %p\n",
								bin->ePhysAddr,
								CE_FIX_ADDRESS(bin->ePhysAddr));
							bin->ePhysAddr = CE_FIX_ADDRESS(bin->ePhysAddr);
						}
					}

					bin->parseState++;
					bin->parseLen = 0;
					bin->parsePtr += sizeof(unsigned int);

					if (bin->parseState == CE_PS_E_DATA) {
						if (bin->ePhysAddr) {
							bin->parsePtr = bin->ePhysAddr;
							bin->parseChkSum = 0;
						} else {
							/* EOF */
							len = 0;
							bin->endOfBin = 1;
						}
					}
				}
				break;

			case CE_PS_E_DATA:
				if (bin->ePhysAddr) {
					copyLen = CE_MIN(bin->ePhysLen - bin->parseLen, len);
					bin->parseLen += copyLen;
					len -= copyLen;

					_debug("copy %d bytes from: %p    to:  %p\n",
						copyLen, pbData, bin->parsePtr);
					while (copyLen--) {
						bin->parseChkSum += *pbData;
						*bin->parsePtr++ = *pbData++;
					}

					if (bin->parseLen == bin->ePhysLen) {
						printf("Section [%02d]: address %p, size 0x%08X, checksum %s\n",
							bin->section,
							bin->ePhysAddr,
							bin->ePhysLen,
							(bin->eChkSum == bin->parseChkSum) ? "ok" : "fail");

						if (bin->eChkSum != bin->parseChkSum) {
							printf("Error: Checksum error, corrupted .BIN file!\n");
							printf("checksum calculated: 0x%08x from file: 0x%08x\n",
								bin->parseChkSum, bin->eChkSum);
							bin->binLen = 0;
							return CE_PR_ERROR;
						}

						bin->section++;
						bin->parseState = CE_PS_E_ADDR;
						bin->parseLen = 0;
						bin->parsePtr = (unsigned char *)&bin->ePhysAddr;
					}
				} else {
					bin->parseLen = 0;
					bin->endOfBin = 1;
					len = 0;
				}
				break;
			}
		}
	}

	if (bin->endOfBin) {
		if (!ce_lookup_ep_bin(bin)) {
			printf("Error: entry point not found!\n");
			bin->binLen = 0;
			return CE_PR_ERROR;
		}

		printf("Entry point: %p, address range: %p-%p\n",
			bin->eEntryPoint,
			bin->rtiPhysAddr,
			bin->rtiPhysAddr + bin->rtiPhysLen);

		return CE_PR_EOF;
	}

	/* Need more data */
	bin->binLen += bin->dataLen;
	return CE_PR_MORE;
}

static int ce_bin_load(void *image, int imglen)
{
	ce_init_bin(&g_bin, image);
	g_bin.dataLen = imglen;
	if (ce_parse_bin(&g_bin) == CE_PR_EOF) {
		ce_prepare_run_bin(&g_bin);
		return 1;
	}

	return 0;
}

typedef void (*CeEntryPointPtr)(void);

static void ce_run_bin(ce_bin *bin)
{
	CeEntryPointPtr EnrtryPoint;

	printf("Launching Windows CE ...\n");

	EnrtryPoint = bin->eEntryPoint;
	EnrtryPoint();
}

static int do_bootce(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	void *addr;
	size_t image_size;
	char *s;

	if (argc > 1) {
		addr = (void *)simple_strtoul(argv[1], NULL, 16);
		image_size = INT_MAX;		/* actually we do not know the image size */
	} else if (getenv("fileaddr") != NULL) {
		addr = (void *)getenv_ulong("fileaddr", 16, 0);
		image_size = getenv_ulong("filesize", 16, INT_MAX);
	} else {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	printf ("## Booting Windows CE Image from address %p ...\n", addr);

	/* check if there is a valid windows CE image */
	if (ce_is_bin_image(addr, image_size)) {
		if (!ce_bin_load(addr, image_size)) {
			/* Ops! Corrupted .BIN image! */
			/* Handle error here ...      */
			printf("corrupted .BIN image !!!\n");
			return 1;
		}
		if ((s = getenv("autostart")) != NULL) {
			if (*s != 'y') {
				/*
				 * just use bootce to load the image to SDRAM;
				 * Do not start it automatically.
				 */
				setenv_addr("fileaddr",
					g_bin.eEntryPoint);
				return 0;
			}
		}
		ce_run_bin(&g_bin);		/* start the image */
	} else {
		printf("Image does not seem to be a valid Windows CE image!\n");
		return 1;
	}
	return 1;	/* never reached - just to keep compiler happy */
}

U_BOOT_CMD(
	bootce,	2,	0,	do_bootce,
	"bootce\t- Boot a Windows CE image from memory \n",
	"[args..]\n"
	"\taddr\t\t-boot image from address addr\n"
);

static void wince_handler(uchar *pkt, unsigned dport, IPaddr_t sip,
			unsigned sport, unsigned len)
{
	NetState = NETLOOP_SUCCESS;	/* got input - quit net loop */

	if (memcmp(&g_net.data[g_net.align_offset],
			eth_get_dev()->enetaddr, ETH_ALEN) == 0) {
		g_net.got_packet_4me = 1;
		g_net.dataLen = len;
	} else {
		g_net.got_packet_4me = 0;
		return;
	}

	g_net.srvAddrRecv.sin_port = *((unsigned short *)(&g_net.data[
			ETHER_HDR_SIZE + IP_HDR_SIZE_NO_UDP + g_net.align_offset]));
	NetCopyIP(&g_net.srvAddrRecv.sin_addr, &g_net.data[ETHER_HDR_SIZE +
		g_net.align_offset + 12]);
	memcpy(NetServerEther, &g_net.data[g_net.align_offset + 6], ETH_ALEN);
#if 0
	printf("received packet:   buffer %p   Laenge %d \n", pkt, len);
	printf("from ");
	print_IPaddr(g_net.srvAddrRecv.sin_addr);
	printf(", port: %d\n", ntohs(g_net.srvAddrRecv.sin_port));

	ce_dump_block(pkt, len);

	printf("Headers:\n");
	ce_dump_block(pkt - ETHER_HDR_SIZE - IP_HDR_SIZE, ETHER_HDR_SIZE + IP_HDR_SIZE);
	printf("my port should be: %d\n",
		ntohs(*((unsigned short *)(&g_net.data[ETHER_HDR_SIZE +
							IP_HDR_SIZE_NO_UDP +
							g_net.align_offset + 2]))));
#endif
}

/* returns packet length if successfull */
static int ce_recv_packet(uchar *buf, int len, struct sockaddr_in *from,
		struct sockaddr_in *local, struct timeval *timeout)
{
	int rxlength;
	ulong time_started;

	g_net.got_packet_4me = 0;
	time_started = get_timer(0);

	NetRxPackets[0] = buf;
	NetSetHandler(wince_handler);

	while (1) {
		rxlength = eth_rx();
		if (g_net.got_packet_4me)
			return g_net.dataLen;
		/* check for timeout */
		if (get_timer(time_started) > timeout->tv_sec * CONFIG_SYS_HZ) {
			return -ETIMEDOUT;
		}
	}
}

static int ce_recv_frame(ce_net *net, int timeout)
{
	struct timeval timeo;

	timeo.tv_sec = timeout;
	timeo.tv_usec = 0;

	net->dataLen = ce_recv_packet(&net->data[net->align_offset],
				sizeof(net->data) - net->align_offset,
				&net->srvAddrRecv, &net->locAddr, &timeo);

	if (net->dataLen < 0 || net->dataLen > sizeof(net->data)) {
		/* Error! No data available */
		net->dataLen = 0;
	}

	return net->dataLen;
}

static int ce_send_frame(ce_net *net)
{
	/* Send UDP packet */
	NetTxPacket = &net->data[net->align_offset];
	return NetSendUDPPacket(NetServerEther, net->srvAddrSend.sin_addr,
				ntohs(net->srvAddrSend.sin_port),
				ntohs(net->locAddr.sin_port), net->dataLen);
}

static int ce_send_write_ack(ce_net *net)
{
	unsigned short *wdata;
	unsigned long aligned_address;

	aligned_address = (unsigned long)&net->data[ETHER_HDR_SIZE + IP_HDR_SIZE + net->align_offset];

	wdata = (unsigned short *)aligned_address;
	wdata[0] = htons(EDBG_CMD_WRITE_ACK);
	wdata[1] = htons(net->blockNum);

	net->dataLen = 4;

	return ce_send_frame(net);
}

static int ce_process_download(ce_net *net, ce_bin *bin)
{
	int ret = CE_PR_MORE;

	if (net->dataLen >= 2) {
		unsigned short command;

		command = ntohs(*(unsigned short *)&net->data[CE_DOFFSET]);
		debug("command found: 0x%04X\n", command);

		switch (command) {
		case EDBG_CMD_WRITE_REQ:
			if (!net->link) {
				// Check file name for WRITE request
				// CE EShell uses "boot.bin" file name
#if 0
				printf(">>>>>>>> First Frame, IP: %s, port: %d\n",
					inet_ntoa((in_addr_t *)&net->srvAddrRecv),
					ntohs(net->srvAddrRecv.sin_port));
#endif
				if (strncmp((char *)&net->data[CE_DOFFSET + 2],
						"boot.bin", 8) == 0) {
					// Some diag output
					if (net->verbose) {
						printf("Locked Down download link, IP: ");
						print_IPaddr(net->srvAddrRecv.sin_addr);
						printf(", port: %d\n", ntohs(net->srvAddrRecv.sin_port));

						printf("Sending BOOTME request [%d] to ",
							net->seqNum);
						print_IPaddr(net->srvAddrSend.sin_addr);
						printf("\n");
					}

					// Lock down EShell download link
					net->locAddr.sin_port = htons(EDBG_DOWNLOAD_PORT + 1);
					net->srvAddrSend.sin_port = net->srvAddrRecv.sin_port;
					net->srvAddrSend.sin_addr = net->srvAddrRecv.sin_addr;
					net->link = 1;
				} else {
					// Unknown link
					net->srvAddrRecv.sin_port = 0;
				}

				if (net->link) {
					ce_send_write_ack(net);
				}
			}
			break;

		case EDBG_CMD_WRITE:
			/* Fix data len */
			bin->dataLen = net->dataLen - 4;

			ret = ce_parse_bin(bin);
			if (ret != CE_PR_ERROR) {
				net->blockNum++;
				ce_send_write_ack(net);
			}
			break;

		case EDBG_CMD_READ_REQ:
			/* Read requests are not supported
			 * Do nothing ...
			 */
			break;

		case EDBG_CMD_ERROR:
			printf("Error: unknown error on the host side\n");

			bin->binLen = 0;
			ret = CE_PR_ERROR;
			break;

		default:
			printf("unknown command 0x%04X\n", command);
			return -EINVAL;
		}
	}
	return ret;
}

static void ce_init_edbg_link(ce_net *net)
{
	/* Initialize EDBG link for commands */

	net->locAddr.sin_port = htons(EDBG_DOWNLOAD_PORT);
	net->srvAddrSend.sin_port = htons(EDBG_DOWNLOAD_PORT);
	net->srvAddrRecv.sin_port = 0;
	net->link = 0;
}

static void ce_process_edbg(ce_net *net, ce_bin *bin)
{
	eth_dbg_hdr *header;

	if (net->dataLen < sizeof(eth_dbg_hdr)) {
		/* Bad packet */

		net->srvAddrRecv.sin_port = 0;
		return;
	}

	header = (eth_dbg_hdr *)&net->data[net->align_offset + ETHER_HDR_SIZE + IP_HDR_SIZE];

	if (header->id != EDBG_ID) {
		/* Bad packet */

		net->srvAddrRecv.sin_port = 0;
		return;
	}

	if (header->service != EDBG_SVC_ADMIN) {
		/* Unknown service */
		return;
	}

	if (!net->link) {
		/* Some diag output */
		if (net->verbose) {
			printf("Locked Down EDBG service link, IP: ");
			print_IPaddr(net->srvAddrRecv.sin_addr);
			printf(", port: %d\n", ntohs(net->srvAddrRecv.sin_port));
		}

		/* Lock down EDBG link */
		net->srvAddrSend.sin_port = net->srvAddrRecv.sin_port;
		net->link = 1;
	}

	switch (header->cmd) {
	case EDBG_CMD_JUMPIMG:
		net->gotJumpingRequest = 1;

		if (net->verbose) {
			printf("Received JUMPING command\n");
		}
		/* Just pass through and copy CONFIG structure */
	case EDBG_CMD_OS_CONFIG:
		/* Copy config structure */
		memcpy(&bin->edbgConfig, header->data,
			sizeof(edbg_os_config_data));
		if (net->verbose) {
			printf("Received CONFIG command\n");
			if (bin->edbgConfig.flags & EDBG_FL_DBGMSG) {
				printf("--> Enabling DBGMSG service, IP: %d.%d.%d.%d, port: %d\n",
					(bin->edbgConfig.dbgMsgIPAddr >> 0) & 0xFF,
					(bin->edbgConfig.dbgMsgIPAddr >> 8) & 0xFF,
					(bin->edbgConfig.dbgMsgIPAddr >> 16) & 0xFF,
					(bin->edbgConfig.dbgMsgIPAddr >> 24) & 0xFF,
					ntohs(bin->edbgConfig.dbgMsgPort));
			}

			if (bin->edbgConfig.flags & EDBG_FL_PPSH) {
				printf("--> Enabling PPSH service, IP: %d.%d.%d.%d, port: %d\n",
					(bin->edbgConfig.ppshIPAddr >> 0) & 0xFF,
					(bin->edbgConfig.ppshIPAddr >> 8) & 0xFF,
					(bin->edbgConfig.ppshIPAddr >> 16) & 0xFF,
					(bin->edbgConfig.ppshIPAddr >> 24) & 0xFF,
					ntohs(bin->edbgConfig.ppshPort));
			}

			if (bin->edbgConfig.flags & EDBG_FL_KDBG) {
				printf("--> Enabling KDBG service, IP: %d.%d.%d.%d, port: %d\n",
					(bin->edbgConfig.kdbgIPAddr >> 0) & 0xFF,
					(bin->edbgConfig.kdbgIPAddr >> 8) & 0xFF,
					(bin->edbgConfig.kdbgIPAddr >> 16) & 0xFF,
					(bin->edbgConfig.kdbgIPAddr >> 24) & 0xFF,
					ntohs(bin->edbgConfig.kdbgPort));
			}

			if (bin->edbgConfig.flags & EDBG_FL_CLEANBOOT) {
				printf("--> Force clean boot\n");
			}
		}
		break;

	default:
		if (net->verbose) {
			printf("Received unknown command: %08X\n", header->cmd);
		}
		return;
	}

	/* Respond with ack */
	header->flags = EDBG_FL_FROM_DEV | EDBG_FL_ACK;
	net->dataLen = EDBG_DATA_OFFSET;
	ce_send_frame(net);
}

static int ce_send_bootme(ce_net *net)
{
	eth_dbg_hdr *header;
	edbg_bootme_data *data;
#ifdef DEBUG
	int	i;
	unsigned char	*pkt;
#endif
	/* Fill out BOOTME packet */
	memset(net->data, 0, PKTSIZE);
	header = (eth_dbg_hdr *)&net->data[CE_DOFFSET];
	data = (edbg_bootme_data *)header->data;

	debug("header=%p data=%p\n", header, data);
	debug("header=%p data=%p\n", (eth_dbg_hdr *)(net->data +CE_DOFFSET),
		(edbg_bootme_data *)header->data);

	header->id = EDBG_ID;
	header->service = EDBG_SVC_ADMIN;
	header->flags = EDBG_FL_FROM_DEV;
	header->seqNum = net->seqNum++;
	header->cmd = EDBG_CMD_BOOTME;

	data->versionMajor = 0;
	data->versionMinor = 0;
	data->cpuId = EDBG_CPU_TYPE_ARM;
	data->bootmeVer = EDBG_CURRENT_BOOTME_VERSION;
	data->bootFlags = 0;
	data->downloadPort = 0;
	data->svcPort = 0;

	/* MAC address from environment*/
	if (!eth_getenv_enetaddr("ethaddr", data->macAddr)) {
		printf("'ethaddr' is not set or invalid\n");
		memset(data->macAddr, 0, sizeof(data->macAddr));
	}

	/* IP address from environment */
	data->ipAddr = (unsigned int)getenv_IPaddr("ipaddr");

	// Device name string (NULL terminated). Should include
	// platform and number based on Ether address (e.g. Odo42, CEPCLS2346, etc)

	// We will use lower MAC address segment to create device name
	// eg. MAC '00-0C-C6-69-09-05', device name 'Triton05'

	strcpy(data->platformId, "Triton");
	sprintf(data->deviceName, "%s%02X", data->platformId, data->macAddr[5]);

#ifdef DEBUG
	printf("header->id: %08X\r\n", header->id);
	printf("header->service: %08X\r\n", header->service);
	printf("header->flags: %08X\r\n", header->flags);
	printf("header->seqNum: %08X\r\n", header->seqNum);
	printf("header->cmd: %08X\r\n\r\n", header->cmd);

	printf("data->versionMajor: %08X\r\n", data->versionMajor);
	printf("data->versionMinor: %08X\r\n", data->versionMinor);
	printf("data->cpuId: %08X\r\n", data->cpuId);
	printf("data->bootmeVer: %08X\r\n", data->bootmeVer);
	printf("data->bootFlags: %08X\r\n", data->bootFlags);
	printf("data->svcPort: %08X\r\n\r\n", ntohs(data->svcPort));

	printf("data->macAddr: %02X-%02X-%02X-%02X-%02X-%02X\r\n",
		data->macAddr[0], data->macAddr[1],
		data->macAddr[2], data->macAddr[3],
		data->macAddr[4], data->macAddr[5]);

	printf("data->ipAddr: %d.%d.%d.%d\r\n",
		(data->ipAddr >> 0) & 0xFF,
		(data->ipAddr >> 8) & 0xFF,
		(data->ipAddr >> 16) & 0xFF,
		(data->ipAddr >> 24) & 0xFF);

	printf("data->platformId: %s\r\n", data->platformId);

	printf("data->deviceName: %s\r\n", data->deviceName);
#endif
	// Some diag output ...
	if (net->verbose) {
		printf("Sending BOOTME request [%d] to ", net->seqNum);
		print_IPaddr(net->srvAddrSend.sin_addr);
		printf("\n");
	}

	net->dataLen = BOOTME_PKT_SIZE;
#ifdef DEBUG
	debug("Start of buffer:      %p\n", net->data);
	debug("Start of ethernet buffer:   %p\n", &net->data[net->align_offset]);
	debug("Start of CE header:         %p\n", header);
	debug("Start of CE data:           %p\n", data);

	pkt = &net->data[net->align_offset];
	debug("packet to send (ceconnect): \n");
	for (i = 0; i < net->dataLen + ETHER_HDR_SIZE + IP_HDR_SIZE; i++) {
		debug("0x%02X ", pkt[i]);
		if (!((i + 1) % 16))
			debug("\n");
	}
	debug("\n");
#endif
	memcpy(NetServerEther, NetBcastAddr, 6);
	return ce_send_frame(net);
}

static void ce_init_download_link(ce_net *net, ce_bin *bin,
				struct sockaddr_in *host_addr, int verbose)
{
	unsigned long aligned_address;

	/* Initialize EDBG link for download */
	memset(net, 0, sizeof(*net));

	/* our buffer contains space for ethernet- ip- and udp- headers */
	/* calculate an offset so that our ce field is aligned to 4 bytes */
	aligned_address = (unsigned long)net->data;
	/* we need 42 bytes room for headers (14 Ethernet , 20 IPv4, 8 UDP) */
	aligned_address += ETHER_HDR_SIZE + IP_HDR_SIZE;
	/* want CE header aligned to 4 Byte boundary */
	net->align_offset = (4 - (aligned_address % 4)) % 4;

	net->locAddr.sin_family = AF_INET;
	net->locAddr.sin_addr = getenv_IPaddr("ipaddr");
	net->locAddr.sin_port = htons(EDBG_DOWNLOAD_PORT);

	net->srvAddrSend.sin_family = AF_INET;
	net->srvAddrSend.sin_port = htons(EDBG_DOWNLOAD_PORT);

	net->srvAddrRecv.sin_family = AF_INET;
	net->srvAddrRecv.sin_port = 0;

	if (host_addr->sin_addr) {
		/* Use specified host address ... */
		net->srvAddrSend.sin_addr = host_addr->sin_addr;
		net->srvAddrRecv.sin_addr = host_addr->sin_addr;
	} else {
		/* ... or default server address */
		net->srvAddrSend.sin_addr = getenv_IPaddr("serverip");
		net->srvAddrRecv.sin_addr = getenv_IPaddr("serverip");
	}

	net->verbose = verbose;

	ce_init_bin(bin, &net->data[CE_DOFFSET + 4]);

	eth_halt();

#ifdef CONFIG_NET_MULTI
	eth_set_current();
#endif
	if (eth_init(gd->bd) < 0) {
#ifdef ET_DEBUG
		puts("ceconnect: failed to init ethernet !\n");
#endif
		eth_halt();
		return;
	}
#ifdef ET_DEBUG
	puts("ceconnect: init ethernet done!\n");
#endif
	memcpy(NetOurEther, eth_get_dev()->enetaddr, ETH_ALEN);
	NetCopyIP(&NetOurIP, &gd->bd->bi_ip_addr);
	NetOurGatewayIP = getenv_IPaddr("gatewayip");
	NetOurSubnetMask = getenv_IPaddr("netmask");
	NetServerIP = getenv_IPaddr("serverip");
}

static int do_ceconnect(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i;
	int verbose = 0, use_timeout = 0;
	int timeout = 0, recv_timeout, ret;
	struct sockaddr_in host_ip_addr;

	for (i = 1; i < argc; i++){
		if (*argv[i] != '-')
			break;
		if (argv[i][1] == 'v') {
			verbose = 1;
		} else if (argv[i][1] == 't') {
			i++;
			if (argc > i) {
				timeout = simple_strtoul(argv[i],
							NULL, 10);
				use_timeout = 1;
			} else {
				printf("Option requires an argument - t\n");
				return 1;
			}
		}
	}

	memset(&host_ip_addr, 0xff, sizeof(host_ip_addr));

	ce_init_download_link(&g_net, &g_bin, &host_ip_addr, verbose);
	while (1) {
		if (g_net.link) {
			recv_timeout = 3;
		} else {
			recv_timeout = 1;

			if (use_timeout && timeout <= 0) {
				printf("CELOAD - Canceled, timeout\n");
				eth_halt();
				return 1;
			}
			if (ctrlc()) {
				printf("CELOAD - canceled by user\n");
				eth_halt();
				return 1;
			}

			debug("sending broadcast frame bootme\n");

			if (ce_send_bootme(&g_net)) {
				printf("CELOAD - error while sending BOOTME request\n");
				eth_halt();
				return 1;
			}
			debug("net state is: %d\n", NetState);
			if (verbose) {
				if (use_timeout) {
					printf("Waiting for connection, timeout %d sec\n", timeout);
				} else {
					printf("Waiting for connection, enter ^C to abort\n");
				}
			}
		}

		if (ce_recv_frame(&g_net, recv_timeout)) {
			ret = ce_process_download(&g_net, &g_bin);
			if (ret != CE_PR_MORE)
				break;
		} else if (use_timeout) {
			timeout -= recv_timeout;
		}
	}

	if (g_bin.binLen) {
		// Try to receive edbg commands from host
		ce_init_edbg_link(&g_net);
		if (verbose)
			printf("Waiting for EDBG commands ...\n");

		while (ce_recv_frame(&g_net, 3))
			ce_process_edbg(&g_net, &g_bin);

		// Prepare WinCE image for execution
		ce_prepare_run_bin(&g_bin);

		// Launch WinCE, if necessary
		if (g_net.gotJumpingRequest)
			ce_run_bin(&g_bin);
	}
	eth_halt();
	return 0;
}

U_BOOT_CMD(
	ceconnect,	4,	1,	do_ceconnect,
	"ceconnect    - Set up a connection to the CE host PC over TCP/IP and download the run-time image\n",
	"ceconnect [-v] [-t <timeout>]\n"
	"  -v verbose operation\n"
	"  -t <timeout> - max wait time (#sec) for the connection\n"
);
