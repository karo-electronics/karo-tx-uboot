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
#include <nand.h>
#include <malloc.h>
#include <asm/errno.h>
#include <jffs2/load_kernel.h>

DECLARE_GLOBAL_DATA_PTR;

#define WINCE_VRAM_BASE		0x80000000
#define CE_FIX_ADDRESS(a)	((void *)((a) - WINCE_VRAM_BASE + CONFIG_SYS_SDRAM_BASE))

#ifndef INT_MAX
#define INT_MAX			((int)(~0U >> 1))
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
static IPaddr_t server_ip;

static void ce_init_bin(ce_bin *bin, unsigned char *dataBuffer)
{
	memset(bin, 0, sizeof(*bin));

	bin->data = dataBuffer;
	bin->parseState = CE_PS_RTI_ADDR;
	bin->parsePtr = (unsigned char *)bin;
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
			.bspVersion = 2,
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

static void ce_setup_std_drv_globals(ce_std_driver_globals *std_drv_glb)
{
	char *mtdparts = getenv("mtdparts");
	size_t max_len = ALIGN((unsigned long)std_drv_glb, SZ_4K) -
		(unsigned long)&std_drv_glb->mtdparts;

	if (eth_get_dev()) {
		memcpy(&std_drv_glb->kitl.mac, eth_get_dev()->enetaddr,
			sizeof(std_drv_glb->kitl.mac));
	}
	snprintf(std_drv_glb->deviceId, sizeof(std_drv_glb->deviceId),
		"Triton%02X", eth_get_dev()->enetaddr[5]);

	NetCopyIP(&std_drv_glb->kitl.ipAddress, &NetOurIP);
	std_drv_glb->kitl.ipMask = getenv_IPaddr("netmask");
	std_drv_glb->kitl.ipRoute = getenv_IPaddr("gatewayip");

	if (mtdparts) {
		strncpy(std_drv_glb->mtdparts, mtdparts, max_len);
		std_drv_glb->mtdparts[max_len - 1] = '\0';
	} else {
		printf("Failed to get mtdparts environment variable\n");
	}
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
			&ce_magic_template, ce_magic,
			(void *)ce_magic + sizeof(*ce_magic) - 1);
		memcpy(ce_magic, &ce_magic_template, sizeof(*ce_magic));

		ce_setup_std_drv_globals(std_drv_glb);
		ce_magic->size = sizeof(*std_drv_glb) +
			strlen(std_drv_glb->mtdparts) + 1;
		ce_dump_block(ce_magic, offsetof(struct ce_magic, drv_glb) +
			ce_magic->size);

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
		debug("got IP address %pI4 from environment\n", &drv_glb->ipAddr);
		debug("got IP mask %pI4 from environment\n", &drv_glb->ipMask);
		debug("got gateway address %pI4 from environment\n", &drv_glb->ipGate);
#endif
		/* EDBG services config */
		memcpy(&drv_glb->edbgConfig, &bin->edbgConfig,
			sizeof(bin->edbgConfig));
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
					if (bin->parseState == CE_PS_RTI_ADDR)
						bin->rtiPhysAddr = CE_FIX_ADDRESS(bin->rtiPhysAddr);
					else if (bin->parseState == CE_PS_E_ADDR &&
						bin->ePhysAddr)
						bin->ePhysAddr = CE_FIX_ADDRESS(bin->ePhysAddr);

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
				debug("ePhysAddr=%p physlen=%08x parselen=%08x\n",
					bin->ePhysAddr, bin->ePhysLen, bin->parseLen);
				if (bin->ePhysAddr) {
					copyLen = CE_MIN(bin->ePhysLen - bin->parseLen, len);
					bin->parseLen += copyLen;
					len -= copyLen;

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

static void ce_run_bin(void (*entry)(void))
{
	printf("Launching Windows CE ...\n");
#ifdef TEST_LAUNCH
return;
#endif
	entry();
}

static int do_bootce(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	void *addr;
	size_t image_size;

	if (argc > 1) {
		addr = (void *)simple_strtoul(argv[1], NULL, 16);
		image_size = INT_MAX;		/* actually we do not know the image size */
	} else if (getenv("fileaddr") != NULL) {
		addr = (void *)getenv_ulong("fileaddr", 16, 0);
		image_size = getenv_ulong("filesize", 16, INT_MAX);
	} else {
		return CMD_RET_USAGE;
	}

	printf ("## Booting Windows CE Image from address %p ...\n", addr);

	/* check if there is a valid windows CE image */
	if (ce_is_bin_image(addr, image_size)) {
		if (!ce_bin_load(addr, image_size)) {
			/* Ops! Corrupted .BIN image! */
			/* Handle error here ...      */
			printf("corrupted .BIN image !!!\n");
			return CMD_RET_FAILURE;
		}
		if (getenv_yesno("autostart") != 1) {
			/*
			 * just use bootce to load the image to SDRAM;
			 * Do not start it automatically.
			 */
			setenv_addr("fileaddr", g_bin.eEntryPoint);
			return CMD_RET_SUCCESS;
		}
		ce_run_bin(g_bin.eEntryPoint);		/* start the image */
	} else {
		printf("Image does not seem to be a valid Windows CE image!\n");
		return CMD_RET_FAILURE;
	}
	return CMD_RET_FAILURE;	/* never reached - just to keep compiler happy */
}
U_BOOT_CMD(
	bootce, 2, 0, do_bootce,
	"bootce [addr]\t- Boot a Windows CE image from memory\n",
	"[args..]\n"
	"\taddr\t\t-boot image from address addr\n"
);

static int ce_nand_load(ce_bin *bin, size_t offset, void *buf, size_t max_len)
{
	int ret;
	size_t len = max_len;

	ret = nand_read_skip_bad(&nand_info[0], offset, &len, buf);
	if (ret < 0) {
		printf("NBOOTCE - aborted\n");
		return ret;
	}
	bin->dataLen = len;
	return len;
}

#define CE_NAND_BUF_SIZE	2048

static int do_nbootce(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	struct mtd_device *dev;
	struct part_info *part_info;
	u8 part_num;
	size_t offset;
	char *end;
	void *buffer;
	size_t len = CE_NAND_BUF_SIZE;

	if (argc < 2 || argc > 3)
		return CMD_RET_USAGE;

	offset = simple_strtoul(argv[1], &end, 16);
	if (*end != '\0') {
		ret = find_dev_and_part(argv[1], &dev, &part_num,
					&part_info);
		if (ret == 0) {
			offset = part_info->offset;
			printf ("## Booting Windows CE Image from NAND partition %s at offset %08x\n",
				argv[1], offset);
		} else {
			printf ("## Booting Windows CE Image from NAND offset %08x\n",
				offset);
		}
	}

	ret = mtdparts_init();
	if (ret)
		return CMD_RET_FAILURE;

	buffer = malloc(CE_NAND_BUF_SIZE);
	if (buffer == NULL) {
		printf("Failed to allocate %u byte buffer\n", CE_NAND_BUF_SIZE);
		return CMD_RET_FAILURE;
	}

	ce_init_bin(&g_bin, buffer);

	ret = ce_nand_load(&g_bin, offset, buffer,
			CE_NAND_BUF_SIZE);
	if (ret < 0) {
		printf("Failed to read NAND: %d\n", ret);
		goto err;
	}
	len = ret;
	/* check if there is a valid windows CE image header */
	if (ce_is_bin_image(buffer, len)) {
		while (ce_parse_bin(&g_bin) == CE_PR_MORE) {
			if (ctrlc()) {
				printf("NBOOTCE - canceled by user\n");
				goto err;
			}
			offset += len;
			ret = ce_nand_load(&g_bin, offset, buffer,
					CE_NAND_BUF_SIZE);
			if (ret < 0)
				goto err;
			len = ret;
		}
		free(buffer);
		if (getenv_yesno("autostart") != 1) {
			/*
			 * just use bootce to load the image to SDRAM;
			 * Do not start it automatically.
			 */
			setenv_addr("fileaddr", g_bin.eEntryPoint);
			return CMD_RET_SUCCESS;
		}
		ce_run_bin(g_bin.eEntryPoint);		/* start the image */
	} else {
		printf("Image does not seem to be a valid Windows CE image!\n");
	}
err:
	free(buffer);
	return CMD_RET_FAILURE;
}
U_BOOT_CMD(
	nbootce, 2, 0, do_nbootce,
	"Boot a Windows CE image from NAND\n",
	"off|partitition\n"
	"\toff\t\t- flash offset (hex)\n"
	"\tpartition\t- partition name\n"
);

static int ce_send_write_ack(ce_net *net)
{
	int ret;
	unsigned short wdata[2];
	int retries = 0;

	wdata[0] = htons(EDBG_CMD_WRITE_ACK);
	wdata[1] = htons(net->blockNum);
	net->dataLen = sizeof(wdata);
	memcpy(net->data, wdata, net->dataLen);

	do {
		ret = bootme_send_frame(net->data, net->dataLen);
		if (ret) {
			printf("Failed to send write ack %d; retries=%d\n",
				ret, retries);
		}
	} while (ret != 0 && retries-- > 0);
	return ret;
}

static enum bootme_state ce_process_download(ce_net *net, ce_bin *bin)
{
	int ret = net->state;

	if (net->dataLen >= 4) {
		unsigned short command;
		unsigned short blknum;

		memcpy(&command, net->data, sizeof(command));
		command = ntohs(command);
		debug("command found: 0x%04X\n", command);

		if (net->state == BOOTME_DOWNLOAD) {
			unsigned short nxt = net->blockNum + 1;

			memcpy(&blknum, &net->data[2], sizeof(blknum));
			blknum = ntohs(blknum);
			if (blknum == nxt) {
				net->blockNum = blknum;
			} else {
				int rc = ce_send_write_ack(net);

				printf("Dropping out of sequence packet with ID %d (expected %d)\n",
					blknum, nxt);
				if (rc != 0)
					return rc;

				return ret;
			}
		}

		switch (command) {
		case EDBG_CMD_WRITE_REQ:
			if (net->state == BOOTME_INIT) {
				// Check file name for WRITE request
				// CE EShell uses "boot.bin" file name
				if (strncmp((char *)&net->data[2],
						"boot.bin", 8) == 0) {
					// Some diag output
					if (net->verbose) {
						printf("Locked Down download link, IP: %pI4\n",
							&NetServerIP);
						printf("Sending BOOTME request [%d] to %pI4\n",
							net->seqNum, &NetServerIP);
					}

					// Lock down EShell download link
					ret = BOOTME_DOWNLOAD;
				} else {
					// Unknown link
					printf("Unknown link\n");
				}

				if (ret == BOOTME_DOWNLOAD) {
					int rc = ce_send_write_ack(net);
					if (rc != 0)
						return rc;
				}
			}
			break;

		case EDBG_CMD_WRITE:
			/* Fixup data len */
			bin->data = &net->data[4];
			bin->dataLen = net->dataLen - 4;
			ret = ce_parse_bin(bin);
			if (ret != CE_PR_ERROR) {
				int rc = ce_send_write_ack(net);
				if (rc)
					return rc;
				if (ret == CE_PR_EOF)
					ret = BOOTME_DONE;
			} else {
				ret = BOOTME_ERROR;
			}
			break;

		case EDBG_CMD_READ_REQ:
			printf("Ignoring EDBG_CMD_READ_REQ\n");
			/* Read requests are not supported
			 * Do nothing ...
			 */
			break;

		case EDBG_CMD_ERROR:
			printf("Error: unknown error on the host side\n");

			bin->binLen = 0;
			ret = BOOTME_ERROR;
			break;

		default:
			printf("unknown command 0x%04X\n", command);
			net->state = BOOTME_ERROR;
		}
	}
	return ret;
}

static enum bootme_state ce_process_edbg(ce_net *net, ce_bin *bin)
{
	enum bootme_state ret = net->state;
	eth_dbg_hdr header;

	if (net->dataLen < sizeof(header)) {
		/* Bad packet */
		printf("Invalid packet size %u\n", net->dataLen);
		net->dataLen = 0;
		return ret;
	}
	memcpy(&header, net->data, sizeof(header));
	if (header.id != EDBG_ID) {
		/* Bad packet */
		printf("Bad EDBG ID %08x\n", header.id);
		net->dataLen = 0;
		return ret;
	}

	if (header.service != EDBG_SVC_ADMIN) {
		/* Unknown service */
		printf("Bad EDBG service %02x\n", header.service);
		net->dataLen = 0;
		return ret;
	}

	if (net->state == BOOTME_INIT) {
		/* Some diag output */
		if (net->verbose) {
			printf("Locked Down EDBG service link, IP: %pI4\n",
				&NetServerIP);
		}

		/* Lock down EDBG link */
		net->state = BOOTME_DEBUG;
	}

debug("%s@%d\n", __func__, __LINE__);
	switch (header.cmd) {
	case EDBG_CMD_JUMPIMG:
debug("%s@%d\n", __func__, __LINE__);
		net->gotJumpingRequest = 1;

		if (net->verbose) {
			printf("Received JUMPING command\n");
		}
		/* Just pass through and copy CONFIG structure */
	case EDBG_CMD_OS_CONFIG:
debug("%s@%d\n", __func__, __LINE__);
		/* Copy config structure */
		memcpy(&bin->edbgConfig, header.data,
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
		ret = BOOTME_DEBUG;
		break;

	default:
		if (net->verbose) {
			printf("Received unknown command: %08X\n", header.cmd);
		}
		return BOOTME_ERROR;
	}

	/* Respond with ack */
	header.flags = EDBG_FL_FROM_DEV | EDBG_FL_ACK;
	net->dataLen = EDBG_DATA_OFFSET;
debug("%s@%d: sending packet %p len %u\n", __func__, __LINE__,
	net->data, net->dataLen);
	bootme_send_frame(net->data, net->dataLen);
	return ret;
}

static enum bootme_state ce_edbg_handler(const void *buf, size_t len)
{
	if (len == 0)
		return BOOTME_DONE;

	g_net.data = (void *)buf;
	g_net.dataLen = len;

	return ce_process_edbg(&g_net, &g_bin);
}

static void ce_init_edbg_link(ce_net *net)
{
	/* Initialize EDBG link for commands */
	net->state = BOOTME_INIT;
}

static enum bootme_state ce_download_handler(const void *buf, size_t len)
{
	g_net.data = (void *)buf;
	g_net.dataLen = len;

	g_net.state = ce_process_download(&g_net, &g_bin);
	return g_net.state;
}

static int ce_send_bootme(ce_net *net)
{
	eth_dbg_hdr *header;
	edbg_bootme_data *data;
	unsigned char txbuf[PKTSIZE_ALIGN];
#ifdef DEBUG
	int	i;
	unsigned char	*pkt;
#endif
	/* Fill out BOOTME packet */
	net->data = txbuf;

	memset(net->data, 0, PKTSIZE);
	header = (eth_dbg_hdr *)net->data;
	data = (edbg_bootme_data *)header->data;

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

	/* IP address from active config */
	NetCopyIP(&data->ipAddr, &NetOurIP);

	// Device name string (NULL terminated). Should include
	// platform and number based on Ether address (e.g. Odo42, CEPCLS2346, etc)

	// We will use lower MAC address segment to create device name
	// eg. MAC '00-0C-C6-69-09-05', device name 'Triton05'

	strncpy(data->platformId, "Triton", sizeof(data->platformId));
	snprintf(data->deviceName, sizeof(data->deviceName), "%s%02X",
		data->platformId, data->macAddr[5]);

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
		printf("Sending BOOTME request [%d] to %pI4\n", net->seqNum,
			&server_ip);
	}

	net->dataLen = BOOTME_PKT_SIZE;
//	net->status = CE_PR_MORE;
	net->state = BOOTME_INIT;
#ifdef DEBUG
	debug("Start of buffer:      %p\n", net->data);
	debug("Start of ethernet buffer:   %p\n", net->data);
	debug("Start of CE header:         %p\n", header);
	debug("Start of CE data:           %p\n", data);

	pkt = net->data;
	debug("packet to send (ceconnect): \n");
	for (i = 0; i < net->dataLen; i++) {
		debug("0x%02X ", pkt[i]);
		if (!((i + 1) % 16))
			debug("\n");
	}
	debug("\n");
#endif
	return BootMeRequest(server_ip, net->data, net->dataLen, 1);
}

static inline int ce_init_download_link(ce_net *net, ce_bin *bin, int verbose)
{
	if (!eth_get_dev()) {
		printf("No network interface available\n");
		return -ENODEV;
	}
	printf("Using device '%s'\n", eth_get_name());

	/* Initialize EDBG link for download */
	memset(net, 0, sizeof(*net));

	net->verbose = verbose;

	/* buffer will be dynamically assigned in ce_download_handler() */
	ce_init_bin(bin, NULL);
	return 0;
}

#define UINT_MAX ~0UL

static inline int ce_download_file(ce_net *net, ulong timeout)
{
	ulong start = get_timer_masked();

	while (net->state == BOOTME_INIT) {
		int ret;

		if (timeout && get_timer(start) > timeout) {
			printf("CELOAD - Canceled, timeout\n");
			return 1;
		}

		if (ctrlc()) {
			printf("CELOAD - canceled by user\n");
			return 1;
		}

		if (ce_send_bootme(&g_net)) {
			printf("CELOAD - error while sending BOOTME request\n");
			return 1;
		}
		if (net->verbose) {
			if (timeout) {
				printf("Waiting for connection, timeout %lu sec\n",
					DIV_ROUND_UP(timeout - get_timer(start),
						CONFIG_SYS_HZ));
			} else {
				printf("Waiting for connection, enter ^C to abort\n");
			}
		}

		ret = BootMeDownload(ce_download_handler);
		if (ret == BOOTME_ERROR) {
			printf("CELOAD - aborted\n");
			return 1;
		}
	}
	return 0;
}

static void ce_disconnect(void)
{
	net_set_udp_handler(NULL);
	eth_halt();
}

static int do_ceconnect(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int verbose = 0;
	ulong timeout = 0;
	int ret = 1;
	int i;

	server_ip = 0;

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
				if (timeout >= UINT_MAX / CONFIG_SYS_HZ) {
					printf("Timeout value %lu out of range (max.: %lu)\n",
						timeout, UINT_MAX / CONFIG_SYS_HZ - 1);
					return CMD_RET_USAGE;
				}
				timeout *= CONFIG_SYS_HZ;
			} else {
				printf("Option requires an argument - t\n");
				return CMD_RET_USAGE;
			}
		} else if (argv[i][1] == 'h') {
			i++;
			if (argc > i) {
				server_ip = string_to_ip(argv[i]);
				printf("Using server %pI4\n", &server_ip);
			} else {
				printf("Option requires an argument - t\n");
				return CMD_RET_USAGE;
			}
		}
	}

	if (ce_init_download_link(&g_net, &g_bin, verbose) != 0)
		goto err;

	if (ce_download_file(&g_net, timeout))
		goto err;

	if (g_bin.binLen) {
		// Try to receive edbg commands from host
		ce_init_edbg_link(&g_net);
		if (verbose)
			printf("Waiting for EDBG commands ...\n");

		ret = BootMeDebugStart(ce_edbg_handler);
		if (ret != BOOTME_DONE)
			goto err;

		// Prepare WinCE image for execution
		ce_prepare_run_bin(&g_bin);

		// Launch WinCE, if necessary
		if (g_net.gotJumpingRequest)
			ce_run_bin(g_bin.eEntryPoint);
	}
	ret = 0;
err:
	ce_disconnect();
	return ret == 0 ? CMD_RET_SUCCESS : CMD_RET_FAILURE;
}
U_BOOT_CMD(
	ceconnect, 6, 1, do_ceconnect,
	"Set up a connection to the CE host PC over TCP/IP and download the run-time image\n",
	"[-v] [-t <timeout>] [-h host]\n"
	"  -v            - verbose operation\n"
	"  -t <timeout>  - max wait time (#sec) for the connection\n"
	"  -h <host>     - send BOOTME requests to <host> (default: broadcast address 255.255.255.255)"
);
