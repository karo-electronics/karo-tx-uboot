#include <config.h>
#include <configs/tx51.h>
#include <asm/arch/imx-regs.h>

#define DCDGEN(type, addr, data)  .long type, addr, data

#define SDRAM_CLK		CONFIG_SYS_SDRAM_CLK

#ifdef PHYS_SDRAM_2_SIZE
#define SDRAM_SIZE		(PHYS_SDRAM_1_SIZE + PHYS_SDRAM_2_SIZE)
#else
#define SDRAM_SIZE		PHYS_SDRAM_1_SIZE
#endif

#define REG_ESDCTL0		0x00
#define REG_ESDCFG0		0x04
#define REG_ESDCTL1		0x08
#define REG_ESDCFG1		0x0c
#define REG_ESDMISC		0x10
#define REG_ESDSCR		0x14
#define REG_ESDGPR		0x34

#define REG_CCGR0		0x68
#define REG_CCGR1		0x6c
#define REG_CCGR2		0x70
#define REG_CCGR3		0x74
#define REG_CCGR4		0x78
#define REG_CCGR5		0x7c
#define REG_CCGR6		0x80
#define REG_CMEOR		0x84

/* SDRAM timing setup */
#define RALAT		1
#define LHD		0

#if SDRAM_SIZE <= SZ_128M
#define RA_BITS		(13 - 11)	/* row addr bits - 11 */
#else
#define RA_BITS		(14 - 11)	/* row addr bits - 11 */
#endif

#define CA_BITS		(10 - 8)	/* 0-2: col addr bits - 8 3: rsrvd */
#define DSIZ		2	/* 0: D[31..16] 1: D[15..D0] 2: D[31..0] 3: rsrvd */
#define SREFR		3	/* 0: disabled 1-5: 2^n rows/clock *: rsrvd */
#define SRT		0	/* 0: disabled *: 1: self refr. ... */
#define PWDT		0	/* 0: disabled 1: precharge pwdn
				   2: pwdn after 64 clocks 3: pwdn after 128 clocks */
#define ESDCTL_VAL	(0x80000000 | (SREFR << 28) | (RA_BITS << 24) | (CA_BITS << 20) | \
			 (DSIZ << 16) | (SRT << 14) | (PWDT << 12))

#define NS_TO_CK(ns)	(((ns) * SDRAM_CLK + 999) / 1000)

	.macro		CK_VAL,	name, clks, offs
	.iflt		\clks - \offs
	.set		\name, 0
	.else
	.set		\name, \clks - \offs
	.endif
	.endm

	.macro		NS_VAL,	name, ns, offs
	.iflt		\ns - \offs
	.set		\name, 0
	.else
	CK_VAL		\name, NS_TO_CK(\ns), \offs
	.endif
	.endm

#if SDRAM_CLK < 200
/* MT46H32M32LF-6 */
NS_VAL	tRFC, 125, 10	/* clks - 10 (0..15) */
NS_VAL	tXSR, 138, 25	/* clks - 25 (0..15) */
NS_VAL	tXP,   25,  1	/* clks - 1 (0..7)  */
CK_VAL	tWTR,   1,  1	/* clks - 1 (0..1)  */
NS_VAL	tRP,   18,  2	/* clks - 2 (0..3)  */
CK_VAL	tMRD,   2,  1	/* clks - 1 (0..3)  */
NS_VAL	tWR,   15,  2	/* clks - 2 (0..1)  */
NS_VAL	tRAS,  42,  1	/* clks - 1 (0..15) */
NS_VAL	tRRD,  12,  1	/* clks - 1 (0..3)  */
NS_VAL	tRCD,  18,  1	/* clks - 1 (0..7) */
NS_VAL	tRC,   60,  1	/* 0: 20 *: clks - 1 (0..15) */
#else
/* MT46H64M32LF-5 or -6 */
NS_VAL	tRFC,  72, 10	/* clks - 10 (0..15) */
NS_VAL	tXSR, 113, 25	/* clks - 25 (0..15) */
CK_VAL	tXP,    2,  1	/* clks - 1 (0..7)  */
CK_VAL	tWTR,   2,  1	/* clks - 1 (0..1)  */
NS_VAL	tRP,   18,  2	/* clks - 2 (0..3)  */
CK_VAL	tMRD,   2,  1	/* clks - 1 (0..3)  */
NS_VAL	tWR,   15,  2	/* clks - 2 (0..1)  */
NS_VAL	tRAS,  42,  1	/* clks - 1 (0..15) */
NS_VAL	tRRD,  12,  1	/* clks - 1 (0..3)  */
NS_VAL	tRCD,  18,  1	/* clks - 1 (0..7) */
NS_VAL	tRC,   60,  1	/* 0: 20 *: clks - 1 (0..15) */
#endif

#define ESDCFG_VAL	((tRFC << 28) | (tXSR << 24) | (tXP << 21) | \
			(tWTR << 20) | (tRP << 18) | (tMRD << 16) | \
			(tRAS << 12) | (tRRD << 10) | (tWR << 7) | \
			(tRCD << 4) | (tRC << 0))

#define ESDMISC_RALAT(n)	(((n) & 0x3) << 7)
#define ESDMISC_DDR2_EN(n)	(((n) & 0x1) << 4)
#define ESDMISC_DDR_EN(n)	(((n) & 0x1) << 3)
#define ESDMISC_AP(n)		(((n) & 0xf) << 16)
#define ESDMISC_VAL		(ESDMISC_AP(10) | ESDMISC_RALAT(RALAT) | \
				(LHD << 5) | ESDMISC_DDR2_EN(0) | ESDMISC_DDR_EN(0))

app_start_addr:
	.long	_start
app_code_barker:
	.long	0xB1
app_code_csf:
	.long	0 // 0x97f40000 - 0x1000
dcd_ptr_ptr:
	.long	dcd_ptr
super_root_key:
	.long	0 // hab_super_root_key
dcd_ptr:
	.long	dcd_data
app_dest_ptr:
	.long	CONFIG_SYS_TEXT_BASE
dcd_data:
	.long	0xB17219E9   // Fixed. can't change.
dcd_len:
	.long	dcd_end - dcd_start
dcd_start:
	DCDGEN(4, CCM_BASE_ADDR + REG_CCGR0, 0xffcffffc);
	DCDGEN(4, CCM_BASE_ADDR + REG_CCGR1, 0x003fffff);
	DCDGEN(4, CCM_BASE_ADDR + REG_CCGR2, 0x030c003c);
	DCDGEN(4, CCM_BASE_ADDR + REG_CCGR3, 0x000000ff);
	DCDGEN(4, CCM_BASE_ADDR + REG_CCGR4, 0x00000000);
	DCDGEN(4, CCM_BASE_ADDR + REG_CCGR5, 0x003fc003);
	DCDGEN(4, CCM_BASE_ADDR + REG_CCGR6, 0x00000000);
	DCDGEN(4, CCM_BASE_ADDR + REG_CMEOR, 0x00000000);

	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDCTL0, 0x80000000)
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDSCR, 0x04008008)
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDSCR, 0x00008010)
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDSCR, 0x00008010)
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDSCR, 0x00338018)
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDCTL0, ESDCTL_VAL)
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDCFG0, ESDCFG_VAL)
#ifdef RAM_BANK1_SIZE
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDCTL1, ESDCTL_VAL)
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDCFG1, ESDCFG_VAL)
#endif
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDGPR, 0x00020000 | ((RALAT & 0x3) << 29))
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDMISC, ESDMISC_VAL)
	DCDGEN(4, ESDCTL_BASE_ADDR + REG_ESDSCR, 0x00000000)

	/* UART1_RXD */
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x228, 0x00000000)
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x618, 0x000001c1)
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x9e4, 0x00000000)
	
	/* UART1_TXD */
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x22c, 0x00000000)
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x61c, 0x000000c5)

	/* UART1_RTS */
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x230, 0x00000000)
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x620, 0x000001c1)
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x9e0, 0x00000000)
	
	/* UART1_CTS */
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x234, 0x00000000)
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x624, 0x000000c5)

	/* STK5 board LED */
	DCDGEN(4, IOMUXC_BASE_ADDR + 0x1d0, 0x00000013)
dcd_end:
	.ifgt	dcd_end - dcd_start - 720
	DCD too large!
	.endif
image_len:
	.long	CONFIG_U_BOOT_IMG_SIZE
