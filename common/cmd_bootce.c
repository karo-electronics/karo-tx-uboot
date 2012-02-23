#include <common.h>
#include <command.h>
#include <net.h>
#include <wince.h>

DECLARE_GLOBAL_DATA_PTR;	/* defines global data structure pointer */


/*/////////////////////////////////////////////////////////////////////////////////////////////*/
/* Local macro */

/* Memory macro */

/* #define CE_RAM_BASE			0x80100000 */
/* #define CE_WINCE_VRAM_BASE	0x80000000 */
/* #define CE_FIX_ADDRESS(a)		(((a) - CE_WINCE_VRAM_BASE) + CE_RAM_BASE) */
#define CE_FIX_ADDRESS(a)		(a)

/* Bin image parse states */

#define CE_PS_RTI_ADDR			0
#define CE_PS_RTI_LEN			1
#define CE_PS_E_ADDR			2
#define CE_PS_E_LEN				3
#define CE_PS_E_CHKSUM			4
#define CE_PS_E_DATA			5

/* Min/max */

#define CE_MIN(a, b)			(((a) < (b)) ? (a) : (b))
#define CE_MAX(a, b)			(((a) > (b)) ? (a) : (b))

// Macro string

#define _STRMAC(s)				#s
#define STRMAC(s)				_STRMAC(s)







///////////////////////////////////////////////////////////////////////////////////////////////
// Global data

static ce_bin __attribute__ ((aligned (32))) g_bin;
static ce_net __attribute__ ((aligned (32))) g_net;


///////////////////////////////////////////////////////////////////////////////////////////////
// Local proto




///////////////////////////////////////////////////////////////////////////////////////////////
// Implementation

int ce_bin_load(void* image, int imglen)
{
	ce_init_bin(&g_bin, image);

	g_bin.dataLen = imglen;

	if (ce_parse_bin(&g_bin) == CE_PR_EOF)
	{
		ce_prepare_run_bin(&g_bin);
		return 1;
	}

	return 0;
}

int ce_is_bin_image(void* image, int imglen)
{
	if (imglen < CE_BIN_SIGN_LEN)
	{
		return 0;
	}

	return (memcmp(image, CE_BIN_SIGN, CE_BIN_SIGN_LEN) == 0);
}

void ce_bin_init_parser(void)
{
	// No buffer address by now, will be specified
	// latter by the ce_bin_parse_next routine

	ce_init_bin(&g_bin, NULL);
}

int ce_bin_parse_next(void* parseBuffer, int len)
{
	int rc;

	g_bin.data = (unsigned char*)parseBuffer;
	g_bin.dataLen = len;
	rc = ce_parse_bin(&g_bin);

	if (rc == CE_PR_EOF)
	{
		ce_prepare_run_bin(&g_bin);
	}

	return rc;
}

void ce_init_bin(ce_bin* bin, unsigned char* dataBuffer)
{
	memset(bin, 0, sizeof(ce_bin));

	bin->data = dataBuffer;
	bin->parseState = CE_PS_RTI_ADDR;
	bin->parsePtr = (unsigned char*)&bin->rtiPhysAddr;
}

int ce_parse_bin(ce_bin* bin)
{
	unsigned char* pbData = bin->data;
	int pbLen = bin->dataLen;
	int copyLen;

	#ifdef DEBUG
	printf("starting ce image parsing:\n\tbin->binLen: 0x%08X\n", bin->binLen);
	printf("\tpbData: 0x%08X        pbLEN: 0x%08X\n", pbData, pbLen);
	#endif

	if (pbLen)
	{
		if (bin->binLen == 0)
		{
			// Check for the .BIN signature first

			if (!ce_is_bin_image(pbData, pbLen))
			{
				printf("Error: Invalid or corrupted .BIN image!\n");

				return CE_PR_ERROR;
			}

			printf("Loading Windows CE .BIN image ...\n");

			// Skip signature

			pbLen -= CE_BIN_SIGN_LEN;
			pbData += CE_BIN_SIGN_LEN;
		}

		while (pbLen)
		{
			switch (bin->parseState)
			{
			case CE_PS_RTI_ADDR:
			case CE_PS_RTI_LEN:
			case CE_PS_E_ADDR:
			case CE_PS_E_LEN:
			case CE_PS_E_CHKSUM:

				copyLen = CE_MIN(sizeof(unsigned int) - bin->parseLen, pbLen);

				memcpy(&bin->parsePtr[ bin->parseLen ], pbData, copyLen);

				bin->parseLen += copyLen;
				pbLen -= copyLen;
				pbData += copyLen;

				if (bin->parseLen == sizeof(unsigned int))
				{
					if (bin->parseState == CE_PS_RTI_ADDR)
					{
						bin->rtiPhysAddr = CE_FIX_ADDRESS(bin->rtiPhysAddr);
					}
					else if (bin->parseState == CE_PS_E_ADDR)
					{
						if (bin->ePhysAddr)
						{
							bin->ePhysAddr = CE_FIX_ADDRESS(bin->ePhysAddr);
						}
					}

					bin->parseState ++;
					bin->parseLen = 0;
					bin->parsePtr += sizeof(unsigned int);

					if (bin->parseState == CE_PS_E_DATA)
					{
						if (bin->ePhysAddr)
						{
							bin->parsePtr = (unsigned char*)(bin->ePhysAddr);
							bin->parseChkSum = 0;
						}
						else
						{
							// EOF

							pbLen = 0;
							bin->endOfBin = 1;
						}
					}
				}

				break;

			case CE_PS_E_DATA:

				if (bin->ePhysAddr)
				{
					copyLen = CE_MIN(bin->ePhysLen - bin->parseLen, pbLen);
					bin->parseLen += copyLen;
					pbLen -= copyLen;

					#ifdef DEBUG
					printf("copy %d bytes from: 0x%08X    to:  0x%08X\n", copyLen, pbData, bin->parsePtr);
					#endif
					while (copyLen --)
					{
						bin->parseChkSum += *pbData;
						*bin->parsePtr ++ = *pbData ++;
					}

					if (bin->parseLen == bin->ePhysLen)
					{
						printf("Section [%02d]: address 0x%08X, size 0x%08X, checksum %s\n",
							bin->secion,
							bin->ePhysAddr,
							bin->ePhysLen,
							(bin->eChkSum == bin->parseChkSum) ? "ok" : "fail");

						if (bin->eChkSum != bin->parseChkSum)
						{
							// Checksum error!

							printf("Error: Checksum error, corrupted .BIN file!\n");

							bin->binLen = 0;

							return CE_PR_ERROR;
						}

						bin->secion ++;
						bin->parseState = CE_PS_E_ADDR;
						bin->parseLen = 0;
						bin->parsePtr = (unsigned char*)(&bin->ePhysAddr);
					}
				}
				else
				{
					bin->parseLen = 0;
					bin->endOfBin = 1;
					pbLen = 0;
				}

				break;
			}
		}
	}

	if (bin->endOfBin)
	{
		// Find entry point

		if (!ce_lookup_ep_bin(bin))
		{
			printf("Error: entry point not found!\n");

			bin->binLen = 0;

			return CE_PR_ERROR;
		}

		printf("Entry point: 0x%08X, address range: 0x%08X-0x%08X\n",
			bin->eEntryPoint,
			bin->rtiPhysAddr,
			bin->rtiPhysAddr + bin->rtiPhysLen);

		return CE_PR_EOF;
	}

	// Need more data

	bin->binLen += bin->dataLen;

	return CE_PR_MORE;
}







void ce_prepare_run_bin(ce_bin* bin)
{
	ce_driver_globals* drv_glb;
	char 	*e, *s;
	char	tmp[64];
	int				i;


	// Clear os RAM area (if needed)

	//if (bin->edbgConfig.flags & EDBG_FL_CLEANBOOT)
	{
		#ifdef DEBUG
		printf("cleaning memory from 0x%08X to 0x%08X\n", bin->eRamStart, bin->eRamStart + bin->eRamLen);
		#endif
		printf("Preparing clean boot ... ");
		memset((void*)bin->eRamStart, 0, bin->eRamLen);
		printf("ok\n");
	}

	// Prepare driver globals (if needed)

	if (bin->eDrvGlb)
	{
		drv_glb = (ce_driver_globals*)bin->eDrvGlb;

		// Fill out driver globals

		memset(drv_glb, 0, sizeof(ce_driver_globals));

		// Signature

		drv_glb->signature = DRV_GLB_SIGNATURE;

		// No flags by now

		drv_glb->flags = 0;

		/* Local ethernet MAC address */
		i = getenv_r ("ethaddr", tmp, sizeof (tmp));
		s = (i > 0) ? tmp : 0;

		for (i = 0; i < 6; ++i) {
			drv_glb->macAddr[i] = s ? simple_strtoul (s, &e, 16) : 0;
			if (s)
				s = (*e) ? e + 1 : e;
		}


		#ifdef DEBUG
		printf("got MAC address %02X:%02X:%02X:%02X:%02X:%02X from environment\n", drv_glb->macAddr[0],drv_glb->macAddr[1],drv_glb->macAddr[2],drv_glb->macAddr[3],drv_glb->macAddr[4],drv_glb->macAddr[5]);
		#endif

		/* Local IP address */
		drv_glb->ipAddr=(unsigned int)getenv_IPaddr("ipaddr");
		#ifdef DEBUG
		printf("got IP address ");
		print_IPaddr((IPaddr_t)drv_glb->ipAddr);
		printf(" from environment\n");
		#endif

		/* Subnet mask */
		drv_glb->ipMask=(unsigned long)getenv_IPaddr("netmask");
		#ifdef DEBUG
		printf("got IP mask ");
		print_IPaddr((IPaddr_t)drv_glb->ipMask);
		printf(" from environment\n");
		#endif

		/* Gateway config */
		drv_glb->ipGate=(unsigned long)getenv_IPaddr("gatewayip");
		#ifdef DEBUG
		printf("got gateway address ");
		print_IPaddr((IPaddr_t)drv_glb->ipGate);
		printf(" from environment\n");
		#endif





		// EDBG services config

		memcpy(&drv_glb->edbgConfig, &bin->edbgConfig, sizeof(bin->edbgConfig));




	}

}


int ce_lookup_ep_bin(ce_bin* bin)
{
	ce_rom_hdr* header;
	ce_toc_entry* tentry;
	e32_rom* e32;
	unsigned int i;

	// Check image Table Of Contents (TOC) signature

	if (*(unsigned int*)(bin->rtiPhysAddr + ROM_SIGNATURE_OFFSET) != ROM_SIGNATURE)
	{
		// Error: Did not find image TOC signature!

		return 0;
	}


	// Lookup entry point

	header = (ce_rom_hdr*)CE_FIX_ADDRESS(*(unsigned int*)(bin->rtiPhysAddr + ROM_SIGNATURE_OFFSET + sizeof(unsigned int)));
	tentry = (ce_toc_entry*)(header + 1);

	for (i = 0; i < header->nummods; i ++)
	{
		// Look for 'nk.exe' module

		if (strcmp((char*)CE_FIX_ADDRESS(tentry[ i ].fileName), "nk.exe") == 0)
		{
			// Save entry point and RAM addresses

			e32 = (e32_rom*)CE_FIX_ADDRESS(tentry[ i ].e32Offset);

			bin->eEntryPoint = CE_FIX_ADDRESS(tentry[ i ].loadOffset) + e32->e32_entryrva;
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




typedef void (*CeEntryPointPtr)(void);



void ce_run_bin(ce_bin* bin)
{
	CeEntryPointPtr EnrtryPoint;

	printf("Launching Windows CE ...\n");


	EnrtryPoint = (CeEntryPointPtr)bin->eEntryPoint;

	EnrtryPoint();

}

int ce_boot (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long	addr;
	unsigned long	image_size;
	unsigned char	*s;


	if (argc < 2) {
		printf ("myUsage:\n%s\n", cmdtp->usage);
		return 1;
	}

	addr = simple_strtoul(argv[1], NULL, 16);
	image_size = 0x7fffffff;		/* actually we do not know the image size */

	printf ("## Booting Windows CE Image from address 0x%08lX ...\n", addr);


	/* check if there is a valid windows CE image */
	if (ce_is_bin_image((void *)addr, image_size))
	{
		if (!ce_bin_load((void*)addr, image_size))
	  	{
		  	/* Ops! Corrupted .BIN image! */
		  	/* Handle error here ...      */
		  	printf("corrupted .BIN image !!!\n");
		  	return 1;

	  	}
	  	if ((s = getenv("autostart")) != NULL) {
			if (*s == 'n') {
				/*
			 	* just use bootce to load the image to SDRAM;
			 	* Do not start it automatically.
			 	*/
			 	return 0;
			}
		}
      	ce_run_bin(&g_bin);		/* start the image */

   	} else {
   		printf("Image seems to be no valid Windows CE image !\n");
   		return 1;

   	}
	return 1;	/* never reached - just to keep compiler happy */


}



U_BOOT_CMD(
	bootce,	2,	0,	ce_boot,
	"bootce\t- Boot a Windows CE image from memory \n",
	"[args..]\n"
	"\taddr\t\t-boot image from address addr\n"
);



#if 1
static void wince_handler (uchar * pkt, unsigned dest, unsigned src, unsigned len)
{


	NetState = NETLOOP_SUCCESS;	/* got input - quit net loop */
	if(!memcmp(g_net.data + g_net.align_offset, gd->bd->bi_enetaddr, 6)) {
		g_net.got_packet_4me=1;
		g_net.dataLen=len;
	} else {
		g_net.got_packet_4me=0;
		return;
	}

	if(1) {
		g_net.srvAddrRecv.sin_port = ntohs(*((unsigned short *)(g_net.data + ETHER_HDR_SIZE + IP_HDR_SIZE_NO_UDP + g_net.align_offset)));
		NetCopyIP(&g_net.srvAddrRecv.sin_addr, g_net.data + ETHER_HDR_SIZE + g_net.align_offset + 12);
		memcpy(NetServerEther, g_net.data + g_net.align_offset +6, 6);

		#if 0
		printf("received packet:   buffer 0x%08X   Laenge %d \n", (unsigned long) pkt, len);
		printf("from ");
		print_IPaddr(g_net.srvAddrRecv.sin_addr);
		printf(", port: %d\n", g_net.srvAddrRecv.sin_port);



		ce_dump_block(pkt, len);

		printf("Headers:\n");
		ce_dump_block(pkt - ETHER_HDR_SIZE - IP_HDR_SIZE, ETHER_HDR_SIZE + IP_HDR_SIZE);
		printf("\n\nmy port should be: %d\n", ntohs(*((unsigned short *)(g_net.data + ETHER_HDR_SIZE + IP_HDR_SIZE_NO_UDP + g_net.align_offset +2))));
		#endif
	}

}



/* returns packet lengt if successfull */
int ce_recv_packet(char *buf, int len, struct sockaddr_in *from, struct sockaddr_in *local, struct timeval *timeout){

	int rxlength;
	ulong time_started;



	g_net.got_packet_4me=0;
	time_started = get_timer(0);


	NetRxPackets[0] = (uchar *)buf;
	NetSetHandler(wince_handler);

	while(1) {
		rxlength=eth_rx();
		if(g_net.got_packet_4me)
			return g_net.dataLen;
		/* check for timeout */
		if (get_timer(time_started) > timeout->tv_sec * CFG_HZ) {
			return -1;
		}
	}
}



int ce_recv_frame(ce_net* net, int timeout)
{
	struct timeval timeo;

	// Setup timeout

	timeo.tv_sec = timeout;
	timeo.tv_usec = 0;

	/* Receive UDP packet */

	net->dataLen = ce_recv_packet(net->data+net->align_offset, sizeof(net->data)-net->align_offset, &net->srvAddrRecv, &net->locAddr, &timeo);

	if (net->dataLen < 0)
	{
		/* Error! No data available */

		net->dataLen = 0;
	}

	return net->dataLen;
}

int ce_process_download(ce_net* net, ce_bin* bin)
{
	int ret = CE_PR_MORE;

	if (net->dataLen >= 2)
	{
		unsigned short command;

		command = ntohs(*(unsigned short*)(net->data+CE_DOFFSET));

		#ifdef DEBUG
		printf("command found: 0x%04X\n", command);
		#endif

		switch (command)
		{
		case EDBG_CMD_WRITE_REQ:

			if (!net->link)
			{
				// Check file name for WRITE request
				// CE EShell uses "boot.bin" file name

				/*printf(">>>>>>>> First Frame, IP: %s, port: %d\n",
							inet_ntoa((in_addr_t *)&net->srvAddrRecv),
							net->srvAddrRecv.sin_port);*/

				if (strncmp((char*)(net->data +CE_DOFFSET + 2), "boot.bin", 8) == 0)
				{
					// Some diag output

					if (net->verbose)
					{
						printf("Locked Down download link, IP: ");
						print_IPaddr(net->srvAddrRecv.sin_addr);
						printf(", port: %d\n", net->srvAddrRecv.sin_port);
					}




					if (net->verbose)
						{
						printf("Sending BOOTME request [%d] to ", (int)net->secNum);
						print_IPaddr(net->srvAddrSend.sin_addr);
						printf("\n");
					}




					// Lock down EShell download link

					net->locAddr.sin_port = (EDBG_DOWNLOAD_PORT + 1);
					net->srvAddrSend.sin_port = net->srvAddrRecv.sin_port;
					net->srvAddrSend.sin_addr = net->srvAddrRecv.sin_addr;
					net->link = 1;
				}
				else
				{
					// Unknown link

					net->srvAddrRecv.sin_port = 0;
				}

				// Return write ack

				if (net->link)
				{
					ce_send_write_ack(net);
				}

				break;
			}

		case EDBG_CMD_WRITE:

			/* Fix data len */
			bin->dataLen = net->dataLen - 4;

			// Parse next block of .bin file

			ret = ce_parse_bin(bin);

			// Request next block

			if (ret != CE_PR_ERROR)
			{
				net->blockNum ++;

				ce_send_write_ack(net);
			}

			break;

		case EDBG_CMD_READ_REQ:

			// Read requests are not supported
			// Do nothing ...

			break;

		case EDBG_CMD_ERROR:

			// Error condition on the host side

			printf("Error: unknown error on the host side\n");

			bin->binLen = 0;
			ret = CE_PR_ERROR;

			break;
		default:
			printf("unknown command 0x%04X ????\n", command);
			while(1);
		}


	}

	return ret;
}



void ce_init_edbg_link(ce_net* net)
{
	/* Initialize EDBG link for commands */

	net->locAddr.sin_port = EDBG_DOWNLOAD_PORT;
	net->srvAddrSend.sin_port = EDBG_DOWNLOAD_PORT;
	net->srvAddrRecv.sin_port = 0;
	net->link = 0;
}

void ce_process_edbg(ce_net* net, ce_bin* bin)
{
	eth_dbg_hdr* header;



	if (net->dataLen < sizeof(eth_dbg_hdr))
	{
		/* Bad packet */

		net->srvAddrRecv.sin_port = 0;
		return;
	}

	header = (eth_dbg_hdr*)(net->data + net->align_offset + ETHER_HDR_SIZE + IP_HDR_SIZE);

	if (header->id != EDBG_ID)
	{
		/* Bad packet */

		net->srvAddrRecv.sin_port = 0;
		return;
	}

	if (header->service != EDBG_SVC_ADMIN)
	{
		/* Unknown service */

		return;
	}

	if (!net->link)
	{
		/* Some diag output */

		if (net->verbose)
		{
			printf("Locked Down EDBG service link, IP: ");
			print_IPaddr(net->srvAddrRecv.sin_addr);
			printf(", port: %d\n", net->srvAddrRecv.sin_port);
		}

		/* Lock down EDBG link */

		net->srvAddrSend.sin_port = net->srvAddrRecv.sin_port;
		net->link = 1;
	}

	switch (header->cmd)
	{
	case EDBG_CMD_JUMPIMG:

		net->gotJumpingRequest = 1;

		if (net->verbose)
		{
			printf("Received JUMPING command\n");
		}

		/* Just pass through and copy CONFIG structure 	*/

	case EDBG_CMD_OS_CONFIG:

		/* Copy config structure */

		memcpy(&bin->edbgConfig, header->data, sizeof(edbg_os_config_data));

		if (net->verbose)
		{
			printf("Received CONFIG command\n");

			if (bin->edbgConfig.flags & EDBG_FL_DBGMSG)
			{
				printf("--> Enabling DBGMSG service, IP: %d.%d.%d.%d, port: %d\n",
					(bin->edbgConfig.dbgMsgIPAddr >> 0) & 0xFF,
					(bin->edbgConfig.dbgMsgIPAddr >> 8) & 0xFF,
					(bin->edbgConfig.dbgMsgIPAddr >> 16) & 0xFF,
					(bin->edbgConfig.dbgMsgIPAddr >> 24) & 0xFF,
					(int)bin->edbgConfig.dbgMsgPort);
			}

			if (bin->edbgConfig.flags & EDBG_FL_PPSH)
			{
				printf("--> Enabling PPSH service, IP: %d.%d.%d.%d, port: %d\n",
					(bin->edbgConfig.ppshIPAddr >> 0) & 0xFF,
					(bin->edbgConfig.ppshIPAddr >> 8) & 0xFF,
					(bin->edbgConfig.ppshIPAddr >> 16) & 0xFF,
					(bin->edbgConfig.ppshIPAddr >> 24) & 0xFF,
					(int)bin->edbgConfig.ppshPort);
			}

			if (bin->edbgConfig.flags & EDBG_FL_KDBG)
			{
				printf("--> Enabling KDBG service, IP: %d.%d.%d.%d, port: %d\n",
					(bin->edbgConfig.kdbgIPAddr >> 0) & 0xFF,
					(bin->edbgConfig.kdbgIPAddr >> 8) & 0xFF,
					(bin->edbgConfig.kdbgIPAddr >> 16) & 0xFF,
					(bin->edbgConfig.kdbgIPAddr >> 24) & 0xFF,
					(int)bin->edbgConfig.kdbgPort);
			}

			if (bin->edbgConfig.flags & EDBG_FL_CLEANBOOT)
			{
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

int ce_send_write_ack(ce_net* net)
{
	unsigned short* wdata;
	unsigned long aligned_address;

	aligned_address=(unsigned long)net->data+ETHER_HDR_SIZE+IP_HDR_SIZE+net->align_offset;

	wdata = (unsigned short*)aligned_address;
	wdata[ 0 ] = htons(EDBG_CMD_WRITE_ACK);
	wdata[ 1 ] = htons(net->blockNum);

	net->dataLen = 4;

	return ce_send_frame(net);
}



int ce_send_frame(ce_net* net)
{
	/* Send UDP packet */
	NetTxPacket = (uchar *)net->data + net->align_offset;
	return NetSendUDPPacket(NetServerEther, net->srvAddrSend.sin_addr, (int)net->srvAddrSend.sin_port, (int)net->locAddr.sin_port, net->dataLen);
}







int ce_send_bootme(ce_net* net)
{
	eth_dbg_hdr* header;
	edbg_bootme_data* data;

	char 	*e, *s;
	int				i;
	unsigned char	tmp[64];
	unsigned char	*macp;

	#ifdef DEBUG
	char	*pkt;
	#endif


	/* Fill out BOOTME packet */
	memset(net->data, 0, PKTSIZE);
	header = (eth_dbg_hdr*)(net->data +CE_DOFFSET);
	data = (edbg_bootme_data*)header->data;

	header->id=EDBG_ID;
	header->service = EDBG_SVC_ADMIN;
	header->flags = EDBG_FL_FROM_DEV;
	header->seqNum = net->secNum ++;
	header->cmd = EDBG_CMD_BOOTME;

	data->versionMajor = 0;
	data->versionMinor = 0;
	data->cpuId = EDBG_CPU_TYPE_ARM;
	data->bootmeVer = EDBG_CURRENT_BOOTME_VERSION;
	data->bootFlags = 0;
	data->downloadPort = 0;
	data->svcPort = 0;

	macp=(unsigned char	*)data->macAddr;
	/* MAC address from environment*/
	i = getenv_r ("ethaddr", tmp, sizeof (tmp));
	s = (i > 0) ? tmp : 0;
	for (i = 0; i < 6; ++i) {
		macp[i] = s ? simple_strtoul (s, &e, 16) : 0;
		if (s)
			s = (*e) ? e + 1 : e;
	}

	/* IP address from environment */
	data->ipAddr = (unsigned int)getenv_IPaddr("ipaddr");

	// Device name string (NULL terminated). Should include
	// platform and number based on Ether address (e.g. Odo42, CEPCLS2346, etc)

	// We will use lower MAC address segment to create device name
	// eg. MAC '00-0C-C6-69-09-05', device name 'Triton05'

	strcpy(data->platformId, "Triton");
	sprintf(data->deviceName, "%s%02X", data->platformId, macp[5]);


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
	printf("data->svcPort: %08X\r\n\r\n", data->svcPort);

	printf("data->macAddr: %02X-%02X-%02X-%02X-%02X-%02X\r\n",
		(data->macAddr[0] >> 0) & 0xFF,
		(data->macAddr[0] >> 8) & 0xFF,
		(data->macAddr[1] >> 0) & 0xFF,
		(data->macAddr[1] >> 8) & 0xFF,
		(data->macAddr[2] >> 0) & 0xFF,
		(data->macAddr[2] >> 8) & 0xFF);

	printf("data->ipAddr: %d.%d.%d.%d\r\n",
		(data->ipAddr >> 0) & 0xFF,
		(data->ipAddr >> 8) & 0xFF,
		(data->ipAddr >> 16) & 0xFF,
		(data->ipAddr >> 24) & 0xFF);

	printf("data->platformId: %s\r\n", data->platformId);

	printf("data->deviceName: %s\r\n", data->deviceName);

#endif


	// Some diag output ...

	if (net->verbose)
	{
		printf("Sending BOOTME request [%d] to ", (int)net->secNum);
		print_IPaddr(net->srvAddrSend.sin_addr);
		printf("\n");
	}

	// Send packet

	net->dataLen = BOOTME_PKT_SIZE;


	#ifdef DEBUG
	printf("\n\n\nStart of buffer:      0x%08X\n", (unsigned long)net->data);
	printf("Start of ethernet buffer:   0x%08X\n", (unsigned long)net->data+net->align_offset);
	printf("Start of CE header:         0x%08X\n", (unsigned long)header);
	printf("Start of CE data:           0x%08X\n", (unsigned long)data);

	pkt = (uchar *)net->data+net->align_offset;
	printf("\n\npacket to send (ceconnect): \n");
	for(i=0; i<(net->dataLen+ETHER_HDR_SIZE+IP_HDR_SIZE); i++) {
		printf("0x%02X ", pkt[i]);
		if(!((i+1)%16))
			printf("\n");
	}
	printf("\n\n");
	#endif

	memcpy(NetServerEther, NetBcastAddr, 6);

	return ce_send_frame(net);
}



void ce_dump_block(unsigned char *ptr, int length) {

	int i;
	int j;

	for(i=0; i<length; i++) {
		if(!(i%16)) {
			printf("\n0x%08X: ", (unsigned long)ptr + i);
		}

		printf("0x%02X ", ptr[i]);

		if(!((i+1)%16)){
			printf("      ");
			for(j=i-15; j<i; j++){
				if((ptr[j]>0x1f) && (ptr[j]<0x7f)) {
					printf("%c", ptr[j]);
				} else {
					printf(".");
				}
			}
		}

	}

	printf("\n\n");
}










void ce_init_download_link(ce_net* net, ce_bin* bin, struct sockaddr_in* host_addr, int verbose)
{
	unsigned long aligned_address;
	/* Initialize EDBG link for download */


	memset(net, 0, sizeof(ce_net));

	/* our buffer contains space for ethernet- ip- and udp- headers */
	/* calucalate an offset that our ce field is aligned to 4 bytes */
	aligned_address=(unsigned long)net->data;			/* this is the start of our physical buffer */
	aligned_address += (ETHER_HDR_SIZE+IP_HDR_SIZE);	/* we need 42 bytes room for headers (14 Ethernet , 20 IPv4, 8 UDP) */
	net->align_offset =	4-(aligned_address%4);			/* want CE header aligned to 4 Byte boundary */
	if(net->align_offset == 4) {
		net->align_offset=0;
	}

	net->locAddr.sin_family = AF_INET;
    net->locAddr.sin_addr = getenv_IPaddr("ipaddr");
    net->locAddr.sin_port = EDBG_DOWNLOAD_PORT;

	net->srvAddrSend.sin_family = AF_INET;
    net->srvAddrSend.sin_port = EDBG_DOWNLOAD_PORT;

	net->srvAddrRecv.sin_family = AF_INET;
    net->srvAddrRecv.sin_port = 0;

	if (host_addr->sin_addr)
	{
		/* Use specified host address ... */

		net->srvAddrSend.sin_addr = host_addr->sin_addr;
		net->srvAddrRecv.sin_addr = host_addr->sin_addr;
	}
	else
	{
		/* ... or use default server address */

		net->srvAddrSend.sin_addr = getenv_IPaddr("serverip");
		net->srvAddrRecv.sin_addr = getenv_IPaddr("serverip");
	}

	net->verbose = 	verbose;
	/* Initialize .BIN parser */
	ce_init_bin(bin, net->data + CE_DOFFSET + 4);



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


	memcpy (NetOurEther, gd->bd->bi_enetaddr, 6);
	NetCopyIP(&NetOurIP, &gd->bd->bi_ip_addr);
	NetOurGatewayIP = getenv_IPaddr ("gatewayip");
	NetOurSubnetMask= getenv_IPaddr ("netmask");
	NetServerIP = getenv_IPaddr ("serverip");

}


int ce_load(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i;
	int verbose, use_timeout;
	int timeout, recv_timeout, ret;
	struct sockaddr_in host_ip_addr;

	// -v verbose

	verbose = 0;
	use_timeout = 0;
	timeout = 0;


	for(i=0;i<argc;i++){
		if (strcmp(argv[i+1], "-v") == 0){
			verbose = 1;
		}
	}


	for(i=0;i<(argc-1);i++){
		if (strcmp(argv[i+1], "-t") == 0){
			use_timeout = 1;
			timeout = simple_strtoul(argv[i+2], NULL, 10);
		}
	}

	#ifdef DEBUG
	printf("verbose=%d, use_timeout=%d, timeout=%d\n", verbose, use_timeout, timeout);
	#endif

	// Check host IP address (if specified)

	*((unsigned int *)&host_ip_addr) = 0xFFFFFFFF;


	// Initialize download link

	ce_init_download_link(&g_net, &g_bin, &host_ip_addr, verbose);

	// Download loop

	while (1)
	{
		if (g_net.link)
		{
			recv_timeout = 3;
		}
		else
		{
			recv_timeout = 1;

			if (use_timeout)
			{
				if (timeout <= 0)
				{
					printf("CELOAD - Canceled, timeout\n");
					eth_halt();
					return 1;
				}
			} else {
				/* Try to catch ^C */
				#ifdef DEBUG
				puts("try to catch ^C\n");
				#endif
				if (ctrlc())
				{
					printf("CELOAD - canceled by user\n");
					eth_halt();
					return 1;
				}
			}
			#ifdef DEBUG
			puts("sending broadcast frame bootme\n");
			#endif

			if (ce_send_bootme(&g_net))
			{
				printf("CELOAD - error while sending BOOTME request\n");
				eth_halt();
				return 1;
			}
			printf("net state is: %d\n", NetState);
			if (verbose)
			{
				if (use_timeout)
				{
					printf("Waiting for connection, timeout %d sec\n", timeout);
				}
				else
				{
					printf("Waiting for connection, enter ^C to abort\n");
				}
			}
		}

		// Try to receive frame

		if (ce_recv_frame(&g_net, recv_timeout))
		{
			// Process received data

			ret = ce_process_download(&g_net, &g_bin);

			if (ret != CE_PR_MORE)
			{
				break;
			}
		}
		else if (use_timeout)
		{
			timeout -= recv_timeout;
		}
	}

	if (g_bin.binLen)
	{
		// Try to receive edbg commands from host

		ce_init_edbg_link(&g_net);

		if (verbose)
		{
			printf("Waiting for EDBG commands ...\n");
		}

		while (ce_recv_frame(&g_net, 3))
		{
			ce_process_edbg(&g_net, &g_bin);
		}

		// Prepare WinCE image for execution

		ce_prepare_run_bin(&g_bin);

		// Launch WinCE, if necessary

		if (g_net.gotJumpingRequest)
		{
			ce_run_bin(&g_bin);
		}
	}
	eth_halt();
	return 0;
}







U_BOOT_CMD(
	ceconnect,	2,	1,	ce_load,
	"ceconnect    - Set up a connection to the CE host PC over TCP/IP and download the run-time image\n",
	"ceconnect [-v] [-t <timeout>]\n"
	"  -v verbose operation\n"
	"  -t <timeout> - max wait time (#sec) for the connection\n"
);

#endif

/* CFG_CMD_WINCE */
