#include <common.h>
#include <cpu_func.h>
#include <image.h>
#include <init.h>
#include <malloc.h>
#include <netdev.h>
#include <dm.h>
#include <dm/platform_data/serial_sh.h>
#include <asm/processor.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/arch/gpio.h>
#include <asm/arch/rmobile.h>
#include <asm/arch/rcar-mstp.h>
#include <asm/arch/sh_sdhi.h>
#include <i2c.h>
#include <mmc.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

#define PFC_BASE	0x11030000

#define ETH_CH0		(PFC_BASE + 0x300c)
#define ETH_CH1		(PFC_BASE + 0x3010)
#define I2C_CH1 	(PFC_BASE + 0x1870)
#define ETH_PVDD_3300	0x00
#define ETH_PVDD_1800	0x01
#define ETH_PVDD_2500	0x02
#define ETH_MII_RGMII	(PFC_BASE + 0x3018)

#define PFC_PWPR		(PFC_BASE + 0x3014)	/* [7] BOWI, [6] PFCWE */
#define PFC_PMC14		(PFC_BASE + 0x214)	/* [0] P4_0 */
#define PFC_PMC15		(PFC_BASE + 0x215)	/* [0] P5_0 */
#define PFC_PFC14		(PFC_BASE + 0x450)	/* [2:0] P4_0 */
#define PFC_PFC15		(PFC_BASE + 0x454)	/* [2:0] P5_0 */
#define PFC_PMC3A		(PFC_BASE + 0x23a)	/* [0] P42_0, [1] P42_1 */
#define PFC_PFC3A		(PFC_BASE + 0x4e8)	/* [2:0] P42_0, [6:4] P42_1 */
#define USBPHY_BASE		(0x11c40000)
#define USB0_BASE		(0x11c50000)
#define USBF_BASE		(0x11c60000)
#define USB1_BASE		(0x11c70000)

/* Reset USB2.0 PHY */
#define USBPHY_RESET		(USBPHY_BASE + 0x000u)
#define USBPHY_UDIRPD		(USBPHY_BASE + 0x01cu)
/* USBTEST registers */
#define RESET			(0x000)
#define UCLKCTL			(0x018)
#define UDIRPD			(0x01c)
#define CON_CTRL		(0x020)
#define CLK_STAT		(0x104)

#define HcRhDescriptorA		(0x048)
#define COMMCTRL		(0x800)
#define LPSTS			(0x102)

/* CPG */
#define CPG_BASE					0x11010000
#define CPG_CLKON_SYC				(CPG_BASE + 0x528)
#define CPG_CLKON_DMAC				(CPG_BASE + 0x578)
#define CPG_CLKON_USB				(CPG_BASE + 0x578)
#define CPG_CLKON_BASE				(CPG_BASE + 0x500)
#define CPG_CLKON_GPIO				(CPG_CLKON_BASE + 0x98)

#define CPG_CLKMON_SYC				(CPG_BASE + 0x6a8)
#define CPG_CLKMON_DMAC				(CPG_BASE + 0x6f8)
#define CPG_CLKMON_USB				(CPG_BASE + 0x6f8)
#define CPG_CLKMON_GPIO				(CPG_BASE + 0x6f8)

#define CPG_RESET_BASE				(CPG_BASE + 0x800)
#define CPG_RESET_ETH				(CPG_RESET_BASE + 0x7C)
#define CPG_RESET_I2C                           (CPG_RESET_BASE + 0x80)
#define CPG_PL2_SDHI_DSEL                           (CPG_BASE + 0x218)
#define CPG_CLK_STATUS                           (CPG_BASE + 0x280)

#define CPG_RST_SYC				(CPG_BASE + 0x828)	/* [0:0] */
#define CPG_RST_DMAC				(CPG_BASE + 0x82c)	/* [2:0] */
#define CPG_RST_USB				(CPG_BASE + 0x878)	/* [3:0] */
#define CPG_RST_GPIO				(CPG_BASE + 0x898)	/* [ : ] */

static int board_cpg_init(void)
{
	printf("%s: ", __func__);
	/* Enable SYC */
	if ((*(volatile u32 *)CPG_CLKMON_SYC) != 0x00000001) {
		(*(volatile u32 *)CPG_RST_SYC) = 0x00010000;
		(*(volatile u32 *)CPG_RST_SYC) = 0x00010001;
		(*(volatile u32 *)CPG_CLKON_SYC) = 0x00010001;
		printf("SYC ");
	}
	/* Enable DMAC */
	if ((*(volatile u32 *)CPG_CLKMON_DMAC) != 0x00000003) {
		(*(volatile u32 *)CPG_RST_DMAC) = 0x00030000;
		(*(volatile u32 *)CPG_RST_DMAC) = 0x00030003;
		(*(volatile u32 *)CPG_CLKON_DMAC) = 0x00030003;
		printf("DMAC ");
	}
	/* Enable GPIO */
	if ((*(volatile u32 *)CPG_CLKMON_GPIO) != 0x00000001) {
		(*(volatile u32 *)CPG_RST_GPIO) = 0x00070007;
		(*(volatile u32 *)CPG_RST_GPIO) = 0x00070007;
		(*(volatile u32 *)CPG_CLKON_GPIO) = 0x00010001;
		printf("GPIO ");
	}
	/* Enable USB */
	if ((*(volatile u32 *)CPG_CLKMON_USB) != 0x0000000f) {
		(*(volatile u32 *)CPG_RST_USB) = 0x000f0000;
		(*(volatile u32 *)CPG_RST_USB) = 0x000f000f;
		(*(volatile u32 *)CPG_CLKON_USB) = 0x000f000f;
		printf("USB ");
	}

	/* some setup time is required */
	printf("\n");

	return 0;
}

static void board_usb_init(void)
{
	/* Setup  */
	/* Disable GPIO Write Protect */
	(*(volatile u32 *)PFC_PWPR) &= ~(0x1u << 7);    /* PWPR.BOWI = 0 */
	(*(volatile u32 *)PFC_PWPR) |= (0x1u << 6);     /* PWPR.PFCWE = 1 */
	(*(volatile u32 *)PFC_PWPR);                    /* barrier */
	__asm__ __volatile__("dsb sy");

	/* Enable USB0 HCD/PCD */
	/* DP/DM are fixed */
	/* set P4_0 as Func.1 for VBUSEN */
#define PFC_PMC14		(PFC_BASE + 0x214)      /* [0] P4_0 */
	(*(volatile u8 *)PFC_PMC14) |= (0x1u << 0);     /* PMC14.b0 = 1 */
	(*(volatile u8 *)PFC_PFC14) &= ~(0x7u << 0);    /* PFC14.PFC0 = 0 */
	(*(volatile u8 *)PFC_PFC14) |= (0x1u << 0);
	/* set P5_0 as Func.1 for OVERCUR */
#define PFC_PMC15		(PFC_BASE + 0x215)      /* [0] P5_0 */
	(*(volatile u8 *)PFC_PMC15) |= (0x1u << 0);     /* PMC15.b0 = 1 */
	(*(volatile u8 *)PFC_PFC15) &= ~(0x7u << 0);    /* PFC15.PFC0 = 0 */
	(*(volatile u8 *)PFC_PFC15) |= (0x1u << 0);

	/* Enable USB1 HCD */
	/* DP/DM are fixed */
	/* set P42_0 as Func.1 for VBUSEN */
	(*(volatile u8 *)PFC_PMC3A) |= (0x1u << 0);     /* PMC14.b0 = 1 */
	(*(volatile u8 *)PFC_PFC3A) &= ~(0xfu << 0);    /* PFC15.PFC0 = 0 */
	(*(volatile u8 *)PFC_PFC3A) |= (0x1u << 0);
	/* set P42_1 as Func.1 for OVERCUR */
	(*(volatile u8 *)PFC_PMC3A) |= (0x1u << 0);     /* PMC14.b1 = 1 */
	(*(volatile u8 *)PFC_PFC3A) &= ~(0xfu << 4);    /* PFC15.PFC1 = 0 */
	(*(volatile u8 *)PFC_PFC3A) |= (0x1u << 4);

	/* Enable write protect */
	/* Enable PFC write protect */
	(*(volatile u32 *)PFC_PWPR) &= ~(0x1u << 6);    /* PWPR.PFCWE = 0 */
	(*(volatile u32 *)PFC_PWPR) |= (0x1u << 7);     /* PWPR.BOWI = 1 */
	(*(volatile u32 *)PFC_PWPR);                    /* barrier */
	__asm__ __volatile__("dsb sy");


	(*(volatile u32 *)USBPHY_RESET) = 0x00001133u;
	udelay(1);
#if 1   /* US0/USB1 use: USB0=OTG, USB1=Host */
	(*(volatile u32 *)USBPHY_RESET) = 0x00001000u;
#endif
#if 0   /* USB1 unuse: USB0=OTG, USB1=USBTEST */
	(*(volatile u32 *)USBPHY_RESET) = 0x00001011u;  /* USB0 only */
#endif
#if 0   /* USB0 unuse: USB0=USBTEST, USB1=OTG */
	(*(volatile u32 *)USBPHY_RESET) = 0x00001000u;  /* USB1 only */
#endif
	udelay(100);

	/* Setup USB0 */
	/* Release USB_BLK module from the standby state (in board_cpg_init) */

	/* Overcurrent function is not supported now */
	(*(volatile u32 *)(USB0_BASE + HcRhDescriptorA)) |= (0x1u << 12);       /* NOCP = 1 */
	/* Select the clock supplid to USBPHY */
	//(*(volatile u32 *)(USBTEST_BASE + UCLKCTL)) =                   // TO BE FIXED
	/* Select host / peripheral operation (USB0 only) */
	//(*(volatile u32 *)(USB0_BASE + COMMCTRL)) = 0;
	/* USB0 is host mode */
	(*(volatile u32 *)(USB0_BASE + COMMCTRL)) = (0x1u << 31);               /* USB0 is function mode */
	/* Set USBPHY normal operation (Function only) */
	(*(volatile u16 *)(USBF_BASE + LPSTS)) |= (0x1u << 14);         /* USBPHY.SUSPM = 1 (func only) */
	/* Select the clock supplid to USBPHY */
	//(*(volatile u32 *)(USBCTR.PLL_RST =                             // TO BE FIXED
	/* wait 100 usec */
	udelay(100);

	/* Setup USB1 */
	/* Release USB_BLK module from the standby state (in board_cpg_init) */

	/* Overcurrent function is not supported now */
	(*(volatile u32 *)(USB1_BASE + HcRhDescriptorA)) |= (0x1u << 12);       /* NOCP = 1 */
	/* Select the clock supplid to USBPHY */
	//(*(volatile u32 *)(USBCTR.PLL_RST =                             // TO BE FIXED
	/* wait 100 usec */
	udelay(100);
}

void s_init(void)
{
	/* can go in board_eht_init() once enabled */
	*(volatile u32 *)(ETH_CH0) = (*(volatile u32 *)(ETH_CH0) & 0xFFFFFFFC) | ETH_PVDD_1800;
	*(volatile u32 *)(ETH_CH1) = (*(volatile u32 *)(ETH_CH1) & 0xFFFFFFFC) | ETH_PVDD_1800;
	/* Enable RGMII for both ETH{0,1} */
	*(volatile u32 *)(ETH_MII_RGMII) = (*(volatile u32 *)(ETH_MII_RGMII) & 0xFFFFFFFC);
	/* ETH CLK */
	*(volatile u32 *)(CPG_RESET_ETH) = 0x30003;
	/* I2C CLK */
	*(volatile u32 *)(CPG_RESET_I2C) = 0xF000F;
	/* I2C pin non GPIO enable */
	*(volatile u32 *)(I2C_CH1) = 0x01010101;
	/* SD CLK */
	*(volatile u32 *)(CPG_PL2_SDHI_DSEL) = 0x00110011;
	while (*(volatile u32 *)(CPG_CLK_STATUS) != 0)
		;
}

int board_early_init_f(void)
{

	return 0;
}

/* PFC */
#define PFC_P37						(PFC_BASE + 0x037)
#define PFC_PM37					(PFC_BASE + 0x16E)
#define PFC_PMC37					(PFC_BASE + 0x237)

#define CONFIG_SYS_SH_SDHI0_BASE	0x11C00000
#define CONFIG_SYS_SH_SDHI1_BASE	0x11C10000
int board_mmc_init(struct bd_info *bis)
{
	int ret = 0;
	
	/* SD1 power control: P39_1 = 0; P39_2 = 1; */
	*(volatile u32 *)(PFC_PMC37) &= 0xFFFFFFF9; /* Port func mode 0b00 */
	*(volatile u32 *)(PFC_PM37) = (*(volatile u32 *)(PFC_PM37) & 0xFFFFFFC3) | 0x28; /* Port output mode 0b1010 */
#if CONFIG_TARGET_SMARC_RZG2L
	*(volatile u32 *)(PFC_P37) = (*(volatile u32 *)(PFC_P37) & 0xFFFFFFF9) | 0x6;	/* Port 39[2:1] output value 0b11*/
#else

	*(volatile u32 *)(PFC_P37) = (*(volatile u32 *)(PFC_P37) & 0xFFFFFFF9) | 0x4;	/* Port 39[2:1] output value 0b10*/
#endif
		
	ret |= sh_sdhi_init(CONFIG_SYS_SH_SDHI0_BASE,
						0,
						SH_SDHI_QUIRK_64BIT_BUF);
	ret |= sh_sdhi_init(CONFIG_SYS_SH_SDHI1_BASE,
						1,
						SH_SDHI_QUIRK_64BIT_BUF);

	return ret;
}


int board_init(void)
{
	/* adress of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_TEXT_BASE + 0x50000;

	board_cpg_init();

	/* PINCTRL, USB-PHY, USB_BLK init */
	board_usb_init();

	return 0;
}

void reset_cpu(ulong addr)
{

}
