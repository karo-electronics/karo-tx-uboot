// SPDX-License-Identifier: GPL-2.0+
/*
 * (c) 2007 Sascha Hauer <s.hauer@pengutronix.de>
 */

#include <common.h>
#include <clk.h>
#include <debug_uart.h>
#include <dm.h>
#include <errno.h>
#include <watchdog.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/clock.h>
#include <asm/global_data.h>
#include <dm/device_compat.h>
#include <dm/platform_data/serial_mxc.h>
#include <serial.h>
#include <linux/compiler.h>
#include <linux/delay.h>

/* UART Control Register Bit Fields.*/
#define URXD_CHARRDY	BIT(15)
#define URXD_ERR	BIT(14)
#define URXD_OVRRUN	BIT(13)
#define URXD_FRMERR	BIT(12)
#define URXD_BRK	BIT(11)
#define URXD_PRERR	BIT(10)
#define URXD_RX_DATA	(0xFF)
#define UCR1_ADEN	BIT(15) /* Auto dectect interrupt */
#define UCR1_ADBR	BIT(14) /* Auto detect baud rate */
#define UCR1_TRDYEN	BIT(13) /* Transmitter ready interrupt enable */
#define UCR1_IDEN	BIT(12) /* Idle condition interrupt */
#define UCR1_RRDYEN	BIT(9)	/* Recv ready interrupt enable */
#define UCR1_RDMAEN	BIT(8)	/* Recv ready DMA enable */
#define UCR1_IREN	BIT(7)	/* Infrared interface enable */
#define UCR1_TXMPTYEN	BIT(6)	/* Transmitter empty interrupt enable */
#define UCR1_RTSDEN	BIT(5)	/* RTS delta interrupt enable */
#define UCR1_SNDBRK	BIT(4)	/* Send break */
#define UCR1_TDMAEN	BIT(3)	/* Transmitter ready DMA enable */
#define UCR1_UARTCLKEN	BIT(2)	/* UART clock enabled */
#define UCR1_DOZE	BIT(1)	/* Doze */
#define UCR1_UARTEN	BIT(0)	/* UART enabled */
#define UCR2_ESCI	BIT(15) /* Escape seq interrupt enable */
#define UCR2_IRTS	BIT(14) /* Ignore RTS pin */
#define UCR2_CTSC	BIT(13) /* CTS pin control */
#define UCR2_CTS	BIT(12) /* Clear to send */
#define UCR2_ESCEN	BIT(11) /* Escape enable */
#define UCR2_PREN	BIT(8)  /* Parity enable */
#define UCR2_PROE	BIT(7)  /* Parity odd/even */
#define UCR2_STPB	BIT(6)	/* Stop */
#define UCR2_WS		BIT(5)	/* Word size */
#define UCR2_RTSEN	BIT(4)	/* Request to send interrupt enable */
#define UCR2_TXEN	BIT(2)	/* Transmitter enabled */
#define UCR2_RXEN	BIT(1)	/* Receiver enabled */
#define UCR2_SRST	BIT(0)	/* SW reset */
#define UCR3_DTREN	BIT(13) /* DTR interrupt enable */
#define UCR3_PARERREN	BIT(12) /* Parity enable */
#define UCR3_FRAERREN	BIT(11) /* Frame error interrupt enable */
#define UCR3_DSR	BIT(10) /* Data set ready */
#define UCR3_DCD	BIT(9)  /* Data carrier detect */
#define UCR3_RI		BIT(8)  /* Ring indicator */
#define UCR3_ADNIMP	BIT(7)  /* Autobaud Detection Not Improved */
#define UCR3_RXDSEN	BIT(6)  /* Receive status interrupt enable */
#define UCR3_AIRINTEN	BIT(5)  /* Async IR wake interrupt enable */
#define UCR3_AWAKEN	BIT(4)  /* Async wake interrupt enable */
#define UCR3_REF25	BIT(3)  /* Ref freq 25 MHz */
#define UCR3_REF30	BIT(2)  /* Ref Freq 30 MHz */

/* imx8 names these bitsfields instead: */
#define UCR3_DTRDEN	BIT(3)  /* bit not used in this chip */
#define UCR3_RXDMUXSEL	BIT(2)  /* RXD muxed input selected; 'should always be set' */

#define UCR3_INVT	BIT(1)  /* Inverted Infrared transmission */
#define UCR3_BPEN	BIT(0)  /* Preset registers enable */
#define UCR4_CTSTL_32	(32<<10) /* CTS trigger level (32 chars) */
#define UCR4_INVR	BIT(9)  /* Inverted infrared reception */
#define UCR4_ENIRI	BIT(8)  /* Serial infrared interrupt enable */
#define UCR4_WKEN	BIT(7)  /* Wake interrupt enable */
#define UCR4_REF16	BIT(6)  /* Ref freq 16 MHz */
#define UCR4_IRSC	BIT(5)  /* IR special case */
#define UCR4_TCEN	BIT(3)  /* Transmit complete interrupt enable */
#define UCR4_BKEN	BIT(2)  /* Break condition interrupt enable */
#define UCR4_OREN	BIT(1)  /* Receiver overrun interrupt enable */
#define UCR4_DREN	BIT(0)  /* Recv data ready interrupt enable */
#define UFCR_RXTL_SHF	0       /* Receiver trigger level shift */
#define UFCR_RFDIV	(7<<7)  /* Reference freq divider mask */
#define UFCR_RFDIV_SHF	7	/* Reference freq divider shift */
#define RFDIV		4	/* divide input clock by 2 */
#define UFCR_DCEDTE	BIT(6)  /* DTE mode select */
#define UFCR_TXTL_SHF	10      /* Transmitter trigger level shift */
#define USR1_PARITYERR	BIT(15) /* Parity error interrupt flag */
#define USR1_RTSS	BIT(14) /* RTS pin status */
#define USR1_TRDY	BIT(13) /* Transmitter ready interrupt/dma flag */
#define USR1_RTSD	BIT(12) /* RTS delta */
#define USR1_ESCF	BIT(11) /* Escape seq interrupt flag */
#define USR1_FRAMERR	BIT(10) /* Frame error interrupt flag */
#define USR1_RRDY	BIT(9)	/* Receiver ready interrupt/dma flag */
#define USR1_TIMEOUT	BIT(7)	/* Receive timeout interrupt status */
#define USR1_RXDS	BIT(6)	/* Receiver idle interrupt flag */
#define USR1_AIRINT	BIT(5)	/* Async IR wake interrupt flag */
#define USR1_AWAKE	BIT(4)	/* Aysnc wake interrupt flag */
#define USR2_ADET	BIT(15) /* Auto baud rate detect complete */
#define USR2_TXFE	BIT(14) /* Transmit buffer FIFO empty */
#define USR2_DTRF	BIT(13) /* DTR edge interrupt flag */
#define USR2_IDLE	BIT(12) /* Idle condition */
#define USR2_IRINT	BIT(8)	/* Serial infrared interrupt flag */
#define USR2_WAKE	BIT(7)	/* Wake */
#define USR2_RTSF	BIT(4)	/* RTS edge interrupt flag */
#define USR2_TXDC	BIT(3)	/* Transmitter complete */
#define USR2_BRCD	BIT(2)	/* Break condition */
#define USR2_ORE	BIT(1)	/* Overrun error */
#define USR2_RDR	BIT(0)	/* Recv data ready */
#define UTS_FRCPERR	BIT(13) /* Force parity error */
#define UTS_LOOP	BIT(12) /* Loop tx and rx */
#define UTS_TXEMPTY	BIT(6)	/* TxFIFO empty */
#define UTS_RXEMPTY	BIT(5)	/* RxFIFO empty */
#define UTS_TXFULL	BIT(4)	/* TxFIFO full */
#define UTS_RXFULL	BIT(3)	/* RxFIFO full */
#define UTS_SOFTRS	BIT(0)	/* Software reset */
#define TXTL		2  /* reset default */
#define RXTL		1  /* reset default */

DECLARE_GLOBAL_DATA_PTR;

struct mxc_uart {
	u32 rxd;
	u32 spare0[15];

	u32 txd;
	u32 spare1[15];

	u32 cr1;
	u32 cr2;
	u32 cr3;
	u32 cr4;

	u32 fcr;
	u32 sr1;
	u32 sr2;
	u32 esc;

	u32 tim;
	u32 bir;
	u32 bmr;
	u32 brc;

	u32 onems;
	u32 ts;
};

static void _mxc_serial_flush(struct mxc_uart *base)
{
	unsigned int timeout = 4000;

	if (!(readl(&base->cr1) & UCR1_UARTEN) ||
	    !(readl(&base->cr2) & UCR2_TXEN))
		return;

	while (!(readl(&base->sr2) & USR2_TXDC) && --timeout)
		udelay(1);
}

static void _mxc_serial_init(struct mxc_uart *base, int use_dte)
{
	_mxc_serial_flush(base);

	writel(0, &base->cr1);
	writel(0, &base->cr2);

	while (!(readl(&base->cr2) & UCR2_SRST));

	if (use_dte)
		writel(0x404 | UCR3_ADNIMP, &base->cr3);
	else
		writel(0x704 | UCR3_ADNIMP, &base->cr3);

	writel(0x704 | UCR3_ADNIMP, &base->cr3);
	writel(0x8000, &base->cr4);
	writel(0x2b, &base->esc);
	writel(0, &base->tim);

	writel(0, &base->ts);
}

static void _mxc_serial_setbrg(struct mxc_uart *base, unsigned long clk,
			       unsigned long baudrate, bool use_dte)
{
	u32 tmp;

	_mxc_serial_flush(base);

	tmp = RFDIV << UFCR_RFDIV_SHF;
	if (use_dte)
		tmp |= UFCR_DCEDTE;
	else
		tmp |= (TXTL << UFCR_TXTL_SHF) | (RXTL << UFCR_RXTL_SHF);
	writel(tmp, &base->fcr);

	writel(0xf, &base->bir);
	writel(clk / (2 * baudrate), &base->bmr);

	writel(UCR2_WS | UCR2_IRTS | UCR2_RXEN | UCR2_TXEN | UCR2_SRST,
	       &base->cr2);

	/*
	 * setting the baudrate triggers a reset, returning cr3 to its
	 * reset value but UCR3_RXDMUXSEL "should always be set."
	 * according to the imx8 reference-manual
	 */
	writel(readl(&base->cr3) | UCR3_RXDMUXSEL, &base->cr3);

	writel(UCR1_UARTEN, &base->cr1);
}

#ifdef CONFIG_DEBUG_UART_MXC
static int init_needed = 1;

static inline void _debug_uart_init(void)
{
	struct mxc_uart *base = (struct mxc_uart *)CONFIG_DEBUG_UART_BASE;

	if (init_needed) {
		_mxc_serial_init(base, false);
		_mxc_serial_setbrg(base, CONFIG_DEBUG_UART_CLOCK,
				   CONFIG_BAUDRATE, false);

		init_needed = 0;
	}
}

static inline void _debug_uart_putc(int ch)
{
	struct mxc_uart *base = (struct mxc_uart *)CONFIG_DEBUG_UART_BASE;

	if (init_needed)
		return;

	while (readl(&base->ts) & UTS_TXFULL)
		WATCHDOG_RESET();

	writel(ch, &base->txd);

	_mxc_serial_flush(base);
}

DEBUG_UART_FUNCS

#endif

#if !CONFIG_IS_ENABLED(DM_SERIAL)

#ifndef CONFIG_MXC_UART_BASE
#error "define CONFIG_MXC_UART_BASE to use the MXC UART driver"
#endif

#define mxc_base	((struct mxc_uart *)CONFIG_MXC_UART_BASE)

static void mxc_serial_setbrg(void)
{
	u32 clk = imx_get_uartclk();

	if (!gd->baudrate)
		gd->baudrate = CONFIG_BAUDRATE;

	_mxc_serial_setbrg(mxc_base, clk, gd->baudrate, false);
}

static int mxc_serial_getc(void)
{
	while (readl(&mxc_base->ts) & UTS_RXEMPTY)
		WATCHDOG_RESET();
	return (readl(&mxc_base->rxd) & URXD_RX_DATA); /* mask out status from upper word */
}

static void mxc_serial_putc(const char c)
{
	/* If \n, also do \r */
	if (c == '\n')
		serial_putc('\r');

	/* wait for transmitter to be ready */
	while (readl(&mxc_base->ts) & UTS_TXFULL)
		WATCHDOG_RESET();

	writel(c, &mxc_base->txd);
}

/*
 * Test whether a character is in the RX buffer
 */
static int one_time_rx_line_always_low_workaround_needed = 1;
static int mxc_serial_tstc(void)
{
	/* If receive fifo is empty, return false */
	if (readl(&mxc_base->ts) & UTS_RXEMPTY)
		return 0;

	/* Empty RX FIFO if receiver is stuck because of RXD line being low */
	if (one_time_rx_line_always_low_workaround_needed) {
		one_time_rx_line_always_low_workaround_needed = 0;
		if (!(readl(&mxc_base->sr2) & USR2_RDR)) {
			while (!(readl(&mxc_base->ts) & UTS_RXEMPTY)) {
				(void) readl(&mxc_base->rxd);
			}
			return 0;
		}
	}

	return 1;
}

/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, 1 start bit.
 */
static int mxc_serial_init(void)
{
	_mxc_serial_init(mxc_base, false);

	serial_setbrg();

	return 0;
}

static int mxc_serial_stop(void)
{
	_mxc_serial_flush(mxc_base);

	return 0;
}

static struct serial_device mxc_serial_drv = {
	.name	= "mxc_serial",
	.start	= mxc_serial_init,
	.stop	= mxc_serial_stop,
	.setbrg	= mxc_serial_setbrg,
	.putc	= mxc_serial_putc,
	.puts	= default_serial_puts,
	.getc	= mxc_serial_getc,
	.tstc	= mxc_serial_tstc,
};

void mxc_serial_initialize(void)
{
	serial_register(&mxc_serial_drv);
}

__weak struct serial_device *default_serial_console(void)
{
	return &mxc_serial_drv;
}
#endif

#if CONFIG_IS_ENABLED(DM_SERIAL)

int mxc_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct mxc_serial_plat *plat = dev_get_plat(dev);
	u32 clk = imx_get_uartclk();

	_mxc_serial_setbrg(plat->reg, clk, baudrate, plat->use_dte);

	return 0;
}

static int mxc_serial_probe(struct udevice *dev)
{
	struct mxc_serial_plat *plat = dev_get_plat(dev);
#if CONFIG_IS_ENABLED(CLK_CCF)
	struct clk *reg_clk = devm_clk_get(dev, "ipg");
	int ret;

	if (IS_ERR(reg_clk)) {
		ret = PTR_ERR(reg_clk);
		if (CONFIG_IS_ENABLED(DEBUG_UART)) {
			debug_uart_init();
			printascii("Failed to get register clock: -");
			printdec(-ret);
			printch('\n');
		}
		dev_err(dev, "Failed to get register clock: %d\n", ret);
		return ret;
	}
	ret = clk_enable(reg_clk);
	if (ret) {
		if (CONFIG_IS_ENABLED(DEBUG_UART)) {
			debug_uart_init();
			printascii("Failed to enable register clock: -");
			printdec(-ret);
			printch('\n');
		}
		dev_err(dev, "%s@%d: Failed to enable register clock: %d\n",
			__func__, __LINE__, ret);
		return ret;
	}
#else
	debug("%s@%d: No clk support\n", __func__, __LINE__);
#endif
	_mxc_serial_init(plat->reg, plat->use_dte);

	return 0;
}

static int mxc_serial_getc(struct udevice *dev)
{
	struct mxc_serial_plat *plat = dev_get_plat(dev);
	struct mxc_uart *const uart = plat->reg;

	if (readl(&uart->ts) & UTS_RXEMPTY)
		return -EAGAIN;

	return readl(&uart->rxd) & URXD_RX_DATA;
}

static int mxc_serial_putc(struct udevice *dev, const char ch)
{
	struct mxc_serial_plat *plat = dev_get_plat(dev);
	struct mxc_uart *const uart = plat->reg;

	if (readl(&uart->ts) & UTS_TXFULL)
		return -EAGAIN;

	writel(ch, &uart->txd);

	return 0;
}

static int mxc_serial_pending(struct udevice *dev, bool input)
{
	struct mxc_serial_plat *plat = dev_get_plat(dev);
	struct mxc_uart *const uart = plat->reg;
	uint32_t sr2 = readl(&uart->sr2);

	if (input)
		return sr2 & USR2_RDR ? 1 : 0;
	else
		return sr2 & USR2_TXDC ? 0 : 1;
}

static const struct dm_serial_ops mxc_serial_ops = {
	.putc = mxc_serial_putc,
	.pending = mxc_serial_pending,
	.getc = mxc_serial_getc,
	.setbrg = mxc_serial_setbrg,
};

#if CONFIG_IS_ENABLED(OF_CONTROL)
static int mxc_serial_of_to_plat(struct udevice *dev)
{
	struct mxc_serial_plat *plat = dev_get_plat(dev);
	fdt_addr_t addr;

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->reg = (struct mxc_uart *)addr;

	plat->use_dte = fdtdec_get_bool(gd->fdt_blob, dev_of_offset(dev),
					"fsl,dte-mode");
	return 0;
}

static const struct udevice_id mxc_serial_ids[] = {
	{ .compatible = "fsl,imx21-uart" },
	{ .compatible = "fsl,imx53-uart" },
	{ .compatible = "fsl,imx6sx-uart" },
	{ .compatible = "fsl,imx6ul-uart" },
	{ .compatible = "fsl,imx7d-uart" },
	{ .compatible = "fsl,imx6q-uart" },
	{ }
};
#endif

U_BOOT_DRIVER(serial_mxc) = {
	.name	= "serial_mxc",
	.id	= UCLASS_SERIAL,
#if CONFIG_IS_ENABLED(OF_CONTROL)
	.of_match = mxc_serial_ids,
	.of_to_plat = mxc_serial_of_to_plat,
	.plat_auto	= sizeof(struct mxc_serial_plat),
#endif
	.probe = mxc_serial_probe,
	.ops	= &mxc_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
#endif

