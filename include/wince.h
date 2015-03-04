/*
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
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
 */
#ifndef __WINCE_H__
#define __WINCE_H__

/* Bin image parse results */
#define CE_PR_EOF	0
#define CE_PR_MORE	1
#define CE_PR_ERROR	2

#pragma pack(1)

/* Edbg BOOTME packet structures */
typedef struct {
	unsigned int	id;		/* Protocol identifier ("EDBG" on the wire) */
	unsigned char	service;	/* Service identifier */
	unsigned char	flags;		/* Flags (see defs below) */
	unsigned char	seqNum;		/* For detection of dropped packets */
	unsigned char	cmd;		/* For administrative messages */
	uchar		data[];		/* Cmd specific data starts here (format is determined by
					 * Cmd, len is determined by UDP packet size)
					 */
} eth_dbg_hdr;

#define OFFSETOF(s,m)					((unsigned int)&(((s*)0)->m))
#define EDBG_DATA_OFFSET				(OFFSETOF(eth_dbg_hdr, data))

typedef struct {
	unsigned char	versionMajor;	/* Bootloader version */
	unsigned char	versionMinor;	/* Bootloader version */
	unsigned char	macAddr[6];	/* Ether address of device (net byte order) */
	unsigned int	ipAddr;		/* IP address of device (net byte order) */
	char		platformId[17];	/* Platform Id string (NULL terminated) */
	char		deviceName[17]; /* Device name string (NULL terminated). Should include
					 * platform and number based on Ether address
					 * (e.g. Odo42, CEPCLS2346, etc)
					 */
	unsigned char	cpuId;		/* CPU identifier (upper nibble = type) */
/* The following fields were added in CE 3.0 Platform Builder release */
	unsigned char	bootmeVer;	/* BOOTME Version.
					 * Must be in the range 2 -> EDBG_CURRENT_BOOTME_VERSION,
					 * or remaining fields will be ignored by Eshell and defaults will be used.
					 */
	unsigned int	bootFlags;	/* Boot Flags */
	unsigned short	downloadPort;	/* Download Port (net byte order) (0 -> EDBG_DOWNLOAD_PORT) */
	unsigned short	svcPort;	/* Service Port (net byte order) (0 -> EDBG_SVC_PORT) */
} edbg_bootme_data;

#define BOOTME_PKT_SIZE			(EDBG_DATA_OFFSET + sizeof(edbg_bootme_data))

// WinCE .BIN file format signature
#define CE_BIN_SIGN	"B000FF\x0A"
#define CE_BIN_SIGN_LEN	7

typedef struct {
	unsigned char sign[CE_BIN_SIGN_LEN];
	unsigned int rtiPhysAddr;
	unsigned int rtiPhysLen;
} ce_bin_hdr;

typedef struct {
	unsigned int physAddr;
	unsigned int physLen;
	unsigned int chkSum;
	unsigned char data[];
} ce_bin_entry;

// CE ROM image structures

#define ROM_SIGNATURE_OFFSET		0x40	/* Offset from the image's physfirst address to the ROM signature. */
#define ROM_SIGNATURE			0x43454345 /* Signature 'CECE' (little endian) */
#define ROM_TOC_POINTER_OFFSET		0x44	/* Offset from the image's physfirst address to the TOC pointer. */
#define ROM_TOC_OFFSET_OFFSET		0x48	/* Offset from the image's physfirst address to the TOC offset (from physfirst). */

typedef struct {
	unsigned int	dllfirst;	/* first DLL address */
	unsigned int	dlllast;	/* last DLL address */
	unsigned int	physfirst;	/* first physical address */
	unsigned int	physlast;	/* highest physical address */
	unsigned int	nummods;	/* number of TOCentry's */
	unsigned int	ramStart;	/* start of RAM */
	unsigned int	ramFree;	/* start of RAM free space */
	unsigned int	ramEnd;		/* end of RAM */
	unsigned int	copyEntries;	/* number of copy section entries */
	unsigned int	copyOffset;	/* offset to copy section */
	unsigned int	profileLen;	/* length of PROFentries RAM */
	unsigned int	profileOffset;	/* offset to PROFentries */
	unsigned int	numfiles;	/* number of FILES */
	unsigned int	kernelFlags;	/* optional kernel flags from ROMFLAGS .bib config option */
	unsigned int	fsRamPercent;	/* Percentage of RAM used for filesystem */
/* from FSRAMPERCENT .bib config option
 * byte 0 = #4K chunks/Mbyte of RAM for filesystem 0-2Mbytes 0-255
 * byte 1 = #4K chunks/Mbyte of RAM for filesystem 2-4Mbytes 0-255
 * byte 2 = #4K chunks/Mbyte of RAM for filesystem 4-6Mbytes 0-255
 * byte 3 = #4K chunks/Mbyte of RAM for filesystem > 6Mbytes 0-255
 */
	unsigned int	drivglobStart;	/* device driver global starting address */
	unsigned int	drivglobLen;	/* device driver global length */
	unsigned short	cpuType;	/* CPU (machine) Type */
	unsigned short	miscFlags;	/* Miscellaneous flags */
	void		*extensions;	/* pointer to ROM Header extensions */
	unsigned int	trackingStart;	/* tracking memory starting address */
	unsigned int	trackingLen;	/* tracking memory ending address */
} ce_rom_hdr;

/* Win32 FILETIME strcuture */
typedef struct {
    unsigned int	loDateTime;
    unsigned int	hiDateTime;
} ce_file_time;

/* Table Of Contents entry structure */
typedef struct {
	unsigned int	fileAttributes;
	ce_file_time	fileTime;
	unsigned int	fileSize;
	char		*fileName;
	unsigned int	e32Offset;	      /* Offset to E32 structure */
	unsigned int	o32Offset;	      /* Offset to O32 structure */
	unsigned int	loadOffset;	      /* MODULE load buffer offset */
} ce_toc_entry;

/* Extra information header block */
typedef struct {
	unsigned int	rva;		/* Virtual relative address of info    */
	unsigned int	size;		/* Size of information block	       */
} e32_info;

#define ROM_EXTRA	9

typedef struct {
	unsigned short  e32_objcnt;     /* Number of memory objects            */
	unsigned short  e32_imageflags; /* Image flags                         */
	unsigned int	e32_entryrva;   /* Relative virt. addr. of entry point */
	unsigned int	e32_vbase;      /* Virtual base address of module      */
	unsigned short  e32_subsysmajor;/* The subsystem major version number  */
	unsigned short  e32_subsysminor;/* The subsystem minor version number  */
	unsigned int	e32_stackmax;   /* Maximum stack size                  */
	unsigned int	e32_vsize;      /* Virtual size of the entire image    */
	unsigned int	e32_sect14rva;  /* section 14 rva */
	unsigned int	e32_sect14size; /* section 14 size */
	unsigned int	e32_timestamp;  /* Time EXE/DLL was created/modified   */
	e32_info	e32_unit[ROM_EXTRA]; /* Array of extra info units      */
	unsigned short  e32_subsys;     /* The subsystem type                  */
} e32_rom;

/* OS config msg */

#define EDBG_FL_DBGMSG    0x01  /* Debug messages */
#define EDBG_FL_PPSH      0x02  /* Text shell */
#define EDBG_FL_KDBG      0x04  /* Kernel debugger */
#define EDBG_FL_CLEANBOOT 0x08  /* Force a clean boot */

typedef struct {
	unsigned char	flags;		 /* Flags that will be used to determine what features are
					  * enabled over ethernet (saved in driver globals by bootloader)
					  */
	unsigned char	kitlTransport;   /* Tells KITL which transport to start */

	/* The following specify addressing info, only valid if the corresponding
	 * flag is set in the Flags field.
	 */
	unsigned int	dbgMsgIPAddr;
	unsigned short	dbgMsgPort;
	unsigned int	ppshIPAddr;
	unsigned short	ppshPort;
	unsigned int	kdbgIPAddr;
	unsigned short	kdbgPort;
} edbg_os_config_data;

/* Driver globals structure
 * Used to pass driver globals info from RedBoot to WinCE core
 */
#define DRV_GLB_SIGNATURE	0x424C4744	/* "DGLB" */
#define STD_DRV_GLB_SIGNATURE	0x53475241	/* "ARGS" */

typedef struct {
	unsigned int	signature;		/* Signature */
	unsigned int	flags;			/* Misc flags */
	unsigned int	ipAddr;			/* IP address of device (net byte order) */
	unsigned int	ipGate;			/* IP address of gateway (net byte order) */
	unsigned int	ipMask;			/* Subnet mask */
	unsigned char	macAddr[6];		/* Ether address of device (net byte order) */
	edbg_os_config_data edbgConfig;		/* EDBG services info */
} ce_driver_globals;

#pragma pack(4)

typedef struct
{
	unsigned long   signature;
	unsigned short  oalVersion;
	unsigned short  bspVersion;
} OAL_ARGS_HEADER;

typedef struct _DEVICE_LOCATION
{
	unsigned long IfcType;
	unsigned long BusNumber;
	unsigned long LogicalLoc;
	void *PhysicalLoc;
	unsigned long Pin;
} DEVICE_LOCATION;

typedef struct
{
	unsigned long flags;
	DEVICE_LOCATION devLoc;
	union {
		struct {
			unsigned long baudRate;
			unsigned long dataBits;
			unsigned long stopBits;
			unsigned long parity;
		};
		struct {
			unsigned short mac[3];
			unsigned long ipAddress;
			unsigned long ipMask;
			unsigned long ipRoute;
		};
	};
} OAL_KITL_ARGS;

typedef struct
{
	OAL_ARGS_HEADER header;
	char		deviceId[16];	// Device identification
	OAL_KITL_ARGS	kitl;
	char		mtdparts[];
} ce_std_driver_globals;

typedef struct {
	void	     *rtiPhysAddr;
	unsigned int rtiPhysLen;
	void	     *ePhysAddr;
	unsigned int ePhysLen;
	unsigned int eChkSum;

	void	     *eEntryPoint;
	void	     *eRamStart;
	unsigned int eRamLen;

	unsigned char parseState;
	unsigned int parseChkSum;
	int parseLen;
	unsigned char *parsePtr;
	int section;

	int dataLen;
	unsigned char *data;

	int binLen;
	int endOfBin;

	edbg_os_config_data edbgConfig;
} ce_bin;

/* IPv4 support */

/* Socket/connection information */
struct sockaddr_in {
	IPaddr_t sin_addr;
	unsigned short sin_port;
	unsigned short sin_family;
	short	       sin_len;
};

#define AF_INET		1
#define INADDR_ANY	0
#ifndef ETH_ALEN
#define ETH_ALEN	6
#endif

enum bootme_state {
	BOOTME_INIT,
	BOOTME_DOWNLOAD,
	BOOTME_DEBUG_INIT,
	BOOTME_DEBUG,
	BOOTME_DONE,
	BOOTME_ERROR,
};

typedef struct {
	int verbose;
	int link;
#ifdef BORKED
	struct sockaddr_in locAddr;
	struct sockaddr_in srvAddrSend;
	struct sockaddr_in srvAddrRecv;
#else
	IPaddr_t server_ip;
#endif
	int gotJumpingRequest;
	int dataLen;
//	int align_offset;
//	int got_packet_4me;
//	int status;
	enum bootme_state state;
	unsigned short blockNum;
	unsigned char seqNum;
	unsigned char pad;
#if 0
	unsigned char data[PKTSIZE_ALIGN];
#else
	unsigned char *data;
#endif
} ce_net;

struct timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

/* Default UDP ports used for Ethernet download and EDBG messages.  May be overriden
 * by device in BOOTME message.
 */
#define  EDBG_DOWNLOAD_PORT				980   /* For downloading images to bootloader via TFTP */
#define  EDBG_SVC_PORT					981   /* Other types of transfers */

/* Byte string for Id field (note - must not conflict with valid TFTP
 * opcodes (0-5), as we share the download port with TFTP)
 */
#define EDBG_ID							0x47424445 /* "EDBG" */

/* Defs for reserved values of the Service field */
#define EDBG_SVC_DBGMSG		0   /* Debug messages */
#define EDBG_SVC_PPSH		1   /* Text shell and PPFS file system */
#define EDBG_SVC_KDBG		2   /* Kernel debugger */
#define EDBG_SVC_ADMIN		0xFF  /* Administrative messages */

/* Commands */
#define EDBG_CMD_READ_REQ	1	/* Read request */
#define EDBG_CMD_WRITE_REQ	2	/* Write request */
#define EDBG_CMD_WRITE		3	/* Host ack */
#define EDBG_CMD_WRITE_ACK	4	/* Target ack */
#define EDBG_CMD_ERROR		5	/* Error */

/* Service Ids from 3-FE are used for user apps */
#define NUM_DFLT_EDBG_SERVICES	3

/* Size of send and receive windows (except for stop and wait mode) */
#define EDBG_WINDOW_SIZE	8

/* The window size can be negotiated up to this amount if a client provides
* enough memory.
 */
#define EDBG_MAX_WINDOW_SIZE	16

/* Max size for an EDBG frame.  Based on ethernet MTU - protocol overhead.
* Limited to one MTU because we don't do IP fragmentation on device.
 */
#define EDBG_MAX_DATA_SIZE	1446

/* Defs for Flags field. */
#define EDBG_FL_FROM_DEV	0x01   /* Set if message is from the device */
#define EDBG_FL_NACK		0x02   /* Set if frame is a nack */
#define EDBG_FL_ACK			0x04   /* Set if frame is an ack */
#define EDBG_FL_SYNC		0x08   /* Can be used to reset sequence # to 0 */
#define EDBG_FL_ADMIN_RESP	0x10	/* For admin messages, indicate whether this is a response */

/* Definitions for Cmd field (used for administrative messages) */
/* Msgs from device */
#define EDBG_CMD_BOOTME		0   /* Initial bootup message from device */

/* Msgs from PC */
#define EDBG_CMD_SETDEBUG	1	/* Used to set debug zones on device (TBD) */
#define EDBG_CMD_JUMPIMG	2	/* Command to tell bootloader to jump to existing
					 * flash or RAM image. Data is same as CMD_OS_CONFIG. */
#define EDBG_CMD_OS_CONFIG	3	/* Configure OS for debug ethernet services */
#define EDBG_CMD_QUERYINFO	4	/* "Ping" device, and return information (same fmt as bootme) */
#define EDBG_CMD_RESET		5	/* Command to have platform perform SW reset (e.g. so it
					 * can be reprogrammed).  Support for this command is
					 * processor dependant, and may not be implemented
					 * on all platforms (requires HW mods for Odo).
					 */
/* Msgs from device or PC */
#define EDBG_CMD_SVC_CONFIG	6
#define EDBG_CMD_SVC_DATA	7

#define EDBG_CMD_DEBUGBREAK	8 /* Break into debugger */

/* Structures for Data portion of EDBG packets */
#define EDBG_MAX_DEV_NAMELEN	16

/* BOOTME message - Devices broadcast this message when booted to request configuration */
#define EDBG_CURRENT_BOOTME_VERSION	2

/*
 * Capability and boot Flags for dwBootFlags in EDBG_BOOTME_DATA
 * LOWORD for boot flags, HIWORD for capability flags
 */

/* Always download image */
#define EDBG_BOOTFLAG_FORCE_DOWNLOAD	0x00000001

/* Support passive-kitl */
#define EDBG_CAPS_PASSIVEKITL		0x00010000

/* Defs for CPUId */
#define EDBG_CPU_TYPE_SHX				0x10
#define EDBG_CPU_TYPE_MIPS				0x20
#define EDBG_CPU_TYPE_X86				0x30
#define EDBG_CPU_TYPE_ARM				0x40
#define EDBG_CPU_TYPE_PPC				0x50
#define EDBG_CPU_TYPE_THUMB				0x60

#define EDBG_CPU_SH3					(EDBG_CPU_TYPE_SHX  | 0)
#define EDBG_CPU_SH4					(EDBG_CPU_TYPE_SHX  | 1)
#define EDBG_CPU_R3000					(EDBG_CPU_TYPE_MIPS | 0)
#define EDBG_CPU_R4101					(EDBG_CPU_TYPE_MIPS | 1)
#define EDBG_CPU_R4102					(EDBG_CPU_TYPE_MIPS | 2)
#define EDBG_CPU_R4111					(EDBG_CPU_TYPE_MIPS | 3)
#define EDBG_CPU_R4200					(EDBG_CPU_TYPE_MIPS | 4)
#define EDBG_CPU_R4300					(EDBG_CPU_TYPE_MIPS | 5)
#define EDBG_CPU_R5230					(EDBG_CPU_TYPE_MIPS | 6)
#define EDBG_CPU_R5432					(EDBG_CPU_TYPE_MIPS | 7)
#define EDBG_CPU_i486					(EDBG_CPU_TYPE_X86  | 0)
#define EDBG_CPU_SA1100					(EDBG_CPU_TYPE_ARM | 0)
#define EDBG_CPU_ARM720					(EDBG_CPU_TYPE_ARM | 1)
#define EDBG_CPU_PPC821					(EDBG_CPU_TYPE_PPC | 0)
#define EDBG_CPU_PPC403					(EDBG_CPU_TYPE_PPC | 1)
#define EDBG_CPU_THUMB720				(EDBG_CPU_TYPE_THUMB | 0)

typedef enum bootme_state bootme_hand_f(const void *pkt, size_t len);

int bootme_recv_frame(void *buf, size_t len, int timeout);
int bootme_send_frame(const void *buf, size_t len);
//void bootme_init(IPaddr_t server_ip);
int BootMeRequest(IPaddr_t server_ip, const void *buf, size_t len, int timeout);
//int ce_download_handler(const void *buf, size_t len);
int BootMeDownload(bootme_hand_f *pkt_handler);
int BootMeDebugStart(bootme_hand_f *pkt_handler);
#endif
