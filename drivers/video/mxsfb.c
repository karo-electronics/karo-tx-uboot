/*
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * LCD driver for i.MXS
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <errno.h>
#include <lcd.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/mxsfb.h>
#include <asm/arch/sys_proto.h>

vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 800,
	.vl_row = 480,

	.vl_bpix = LCD_COLOR24,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
};

static int bits_per_pixel;
static int color_depth;
static uint32_t pix_fmt;
static struct fb_var_screeninfo mxsfb_var;

static struct mxs_lcdif_regs *lcd_regs = (void *)MXS_LCDIF_BASE;

void *lcd_base;			/* Start of framebuffer memory	*/
void *lcd_console_address;	/* Start of console buffer	*/

int lcd_line_length;
int lcd_color_fg;
int lcd_color_bg;

short console_col;
short console_row;

void lcd_initcolregs(void)
{
}

void lcd_setcolreg(ushort regno, ushort red, ushort green, ushort blue)
{
}

#define fourcc_str(fourcc)	((fourcc) >> 0) & 0xff, \
		((fourcc) >> 8) & 0xff,			\
		((fourcc) >> 16) & 0xff,		\
		((fourcc) >> 24) & 0xff

#define LCD_CTRL_DEFAULT	(LCDIF_CTRL_LCDIF_MASTER |	\
				LCDIF_CTRL_BYPASS_COUNT |	\
				LCDIF_CTRL_DOTCLK_MODE)

#define LCD_CTRL1_DEFAULT	0
#define LCD_CTRL2_DEFAULT	LCDIF_CTRL2_OUTSTANDING_REQS_REQ_2

#define LCD_VDCTRL0_DEFAULT	(LCDIF_VDCTRL0_ENABLE_PRESENT |		\
				LCDIF_VDCTRL0_VSYNC_PERIOD_UNIT |	\
				LCDIF_VDCTRL0_VSYNC_PULSE_WIDTH_UNIT)
#define LCD_VDCTRL1_DEFAULT	0
#define LCD_VDCTRL2_DEFAULT	0
#define LCD_VDCTRL3_DEFAULT	0
#define LCD_VDCTRL4_DEFAULT	LCDIF_VDCTRL4_SYNC_SIGNALS_ON

void video_hw_init(void *lcdbase)
{
	int ret;
	unsigned int div = 0, best = 0, pix_clk;
	u32 frac1;
	const unsigned long lcd_clk = 480000000;
	u32 lcd_ctrl = LCD_CTRL_DEFAULT | LCDIF_CTRL_RUN;
	u32 lcd_ctrl1 = LCD_CTRL1_DEFAULT, lcd_ctrl2 = LCD_CTRL2_DEFAULT;
	u32 lcd_vdctrl0 = LCD_VDCTRL0_DEFAULT;
	u32 lcd_vdctrl1 = LCD_VDCTRL1_DEFAULT;
	u32 lcd_vdctrl2 = LCD_VDCTRL2_DEFAULT;
	u32 lcd_vdctrl3 = LCD_VDCTRL3_DEFAULT;
	u32 lcd_vdctrl4 = LCD_VDCTRL4_DEFAULT;
	struct mxs_clkctrl_regs *clk_regs = (void *)MXS_CLKCTRL_BASE;
	char buf1[16], buf2[16];

	/* pixel format in memory */
	switch (color_depth) {
	case 8:
		lcd_ctrl |= LCDIF_CTRL_WORD_LENGTH_8BIT;
		lcd_ctrl1 |= LCDIF_CTRL1_BYTE_PACKING_FORMAT(1);
		break;

	case 16:
		lcd_ctrl |= LCDIF_CTRL_WORD_LENGTH_16BIT;
		lcd_ctrl1 |= LCDIF_CTRL1_BYTE_PACKING_FORMAT(3);
		break;

	case 18:
		lcd_ctrl |= LCDIF_CTRL_WORD_LENGTH_18BIT;
		lcd_ctrl1 |= LCDIF_CTRL1_BYTE_PACKING_FORMAT(7);
		break;

	case 24:
		lcd_ctrl |= LCDIF_CTRL_WORD_LENGTH_24BIT;
		lcd_ctrl1 |= LCDIF_CTRL1_BYTE_PACKING_FORMAT(7);
		break;

	default:
		printf("Invalid bpp: %d\n", color_depth);
		return;
	}

	/* pixel format on the LCD data pins */
	switch (pix_fmt) {
	case PIX_FMT_RGB332:
		lcd_ctrl |= LCDIF_CTRL_LCD_DATABUS_WIDTH_8BIT;
		break;

	case PIX_FMT_RGB565:
		lcd_ctrl |= LCDIF_CTRL_LCD_DATABUS_WIDTH_16BIT;
		break;

	case PIX_FMT_BGR666:
		lcd_ctrl |= 1 << LCDIF_CTRL_INPUT_DATA_SWIZZLE_OFFSET;
		/* fallthru */
	case PIX_FMT_RGB666:
		lcd_ctrl |= LCDIF_CTRL_LCD_DATABUS_WIDTH_18BIT;
		break;

	case PIX_FMT_BGR24:
		lcd_ctrl |= 1 << LCDIF_CTRL_INPUT_DATA_SWIZZLE_OFFSET;
		/* fallthru */
	case PIX_FMT_RGB24:
		lcd_ctrl |= LCDIF_CTRL_LCD_DATABUS_WIDTH_24BIT;
		break;

	default:
		printf("Invalid pixel format: %c%c%c%c\n", fourcc_str(pix_fmt));
		return;
	}

	pix_clk = PICOS2KHZ(mxsfb_var.pixclock);
	debug("designated pix_clk: %sMHz\n", strmhz(buf1, pix_clk * 1000));

	for (frac1 = 18; frac1 < 36; frac1++) {
		static unsigned int err = ~0;
		unsigned long clk = lcd_clk / 1000 * 18 / frac1;
		unsigned int d = (clk + pix_clk - 1) / pix_clk;
		unsigned int diff = abs(clk / d - pix_clk);

		debug("frac1=%u div=%u lcd_clk=%-8sMHz pix_clk=%-8sMHz diff=%u err=%u\n",
			frac1, d, strmhz(buf1, clk * 1000), strmhz(buf2, clk * 1000 / d),
			diff, err);

		if (clk < pix_clk)
			break;
		if (d > 255)
			continue;

		if (diff < err) {
			best = frac1;
			div = d;
			err = diff;
			if (err == 0)
				break;
		}
	}
	if (div == 0) {
		printf("Requested pixel clock %sMHz out of range\n",
			strmhz(buf1, pix_clk * 1000));
		return;
	}

	debug("div=%lu(%u*%u/18) for pixel clock %sMHz with base clock %sMHz\n",
		lcd_clk / pix_clk / 1000, best, div,
		strmhz(buf1, lcd_clk / div * 18 / best),
		strmhz(buf2, lcd_clk));

	frac1 = (readl(&clk_regs->hw_clkctrl_frac1_reg) & ~0xff) | best;
	writel(frac1, &clk_regs->hw_clkctrl_frac1_reg);
	writel(1 << 14, &clk_regs->hw_clkctrl_clkseq_clr);

	/* enable LCD clk and fractional divider */
	writel(div, &clk_regs->hw_clkctrl_lcdif_reg);
	while (readl(&clk_regs->hw_clkctrl_lcdif_reg) & (1 << 29))
		;

	ret = mxs_reset_block(&lcd_regs->hw_lcdif_ctrl_reg);
	if (ret) {
		printf("Failed to reset LCD controller: LCDIF_CTRL: %08x CLKCTRL_LCDIF: %08x\n",
			readl(&lcd_regs->hw_lcdif_ctrl_reg),
			readl(&clk_regs->hw_clkctrl_lcdif_reg));
		return;
	}

	if (mxsfb_var.sync & FB_SYNC_HOR_HIGH_ACT)
		lcd_vdctrl0 |= LCDIF_VDCTRL0_HSYNC_POL;

	if (mxsfb_var.sync & FB_SYNC_VERT_HIGH_ACT)
		lcd_vdctrl0 |= LCDIF_VDCTRL0_HSYNC_POL;

	if (mxsfb_var.sync & FB_SYNC_DATA_ENABLE_HIGH_ACT)
		lcd_vdctrl0 |= LCDIF_VDCTRL0_ENABLE_POL;

	if (mxsfb_var.sync & FB_SYNC_DOTCLK_FALLING_ACT)
		lcd_vdctrl0 |= LCDIF_VDCTRL0_DOTCLK_POL;

	lcd_vdctrl0 |= LCDIF_VDCTRL0_VSYNC_PULSE_WIDTH(mxsfb_var.vsync_len);
	lcd_vdctrl1 |= LCDIF_VDCTRL1_VSYNC_PERIOD(mxsfb_var.vsync_len +
						mxsfb_var.upper_margin +
						mxsfb_var.lower_margin +
						mxsfb_var.yres);
	lcd_vdctrl2 |= LCDIF_VDCTRL2_HSYNC_PULSE_WIDTH(mxsfb_var.hsync_len);
	lcd_vdctrl2 |= LCDIF_VDCTRL2_HSYNC_PERIOD(mxsfb_var.hsync_len +
						mxsfb_var.left_margin +
						mxsfb_var.right_margin +
						mxsfb_var.xres);

	lcd_vdctrl3 |= LCDIF_VDCTRL3_HORIZONTAL_WAIT_CNT(mxsfb_var.left_margin +
							mxsfb_var.hsync_len);
	lcd_vdctrl3 |= LCDIF_VDCTRL3_VERTICAL_WAIT_CNT(mxsfb_var.upper_margin +
							mxsfb_var.vsync_len);

	lcd_vdctrl4 |= LCDIF_VDCTRL4_DOTCLK_H_VALID_DATA_CNT(mxsfb_var.xres);

	writel((u32)lcdbase, &lcd_regs->hw_lcdif_next_buf_reg);
	writel(LCDIF_TRANSFER_COUNT_H_COUNT(mxsfb_var.xres) |
		LCDIF_TRANSFER_COUNT_V_COUNT(mxsfb_var.yres),
		&lcd_regs->hw_lcdif_transfer_count_reg);

	writel(lcd_vdctrl0, &lcd_regs->hw_lcdif_vdctrl0_reg);
	writel(lcd_vdctrl1, &lcd_regs->hw_lcdif_vdctrl1_reg);
	writel(lcd_vdctrl2, &lcd_regs->hw_lcdif_vdctrl2_reg);
	writel(lcd_vdctrl3, &lcd_regs->hw_lcdif_vdctrl3_reg);
	writel(lcd_vdctrl4, &lcd_regs->hw_lcdif_vdctrl4_reg);

	writel(lcd_ctrl1, &lcd_regs->hw_lcdif_ctrl1_reg);
	writel(lcd_ctrl2, &lcd_regs->hw_lcdif_ctrl2_reg);

	writel(lcd_ctrl, &lcd_regs->hw_lcdif_ctrl_reg);

	debug("mxsfb framebuffer driver initialized\n");
}

void mxsfb_disable(void)
{
	u32 lcd_ctrl = readl(&lcd_regs->hw_lcdif_ctrl_reg);

	writel(lcd_ctrl & ~LCDIF_CTRL_RUN, &lcd_regs->hw_lcdif_ctrl_reg);
}

int mxsfb_init(struct fb_videomode *mode, uint32_t pixfmt, int bpp)
{
	switch (bpp) {
	case 8:
		bits_per_pixel = 8;
		panel_info.vl_bpix = LCD_COLOR8;
		break;

	case 16:
		bits_per_pixel = 16;
		panel_info.vl_bpix = LCD_COLOR16;
		break;

	case 18:
		bits_per_pixel = 32;
		panel_info.vl_bpix = LCD_COLOR24;
		break;

	case 24:
		bits_per_pixel = 32;
		panel_info.vl_bpix = LCD_COLOR24;
		break;

	default:
		return -EINVAL;
	}

	pix_fmt = pixfmt;
	color_depth = bpp;

	lcd_line_length = bits_per_pixel / 8 * mode->xres;

	mxsfb_var.xres = mode->xres;
	mxsfb_var.yres = mode->yres;
	mxsfb_var.xres_virtual = mode->xres;
	mxsfb_var.yres_virtual = mode->yres;
	mxsfb_var.pixclock = mode->pixclock;
	mxsfb_var.left_margin = mode->left_margin;
	mxsfb_var.right_margin = mode->right_margin;
	mxsfb_var.upper_margin = mode->upper_margin;
	mxsfb_var.lower_margin = mode->lower_margin;
	mxsfb_var.hsync_len = mode->hsync_len;
	mxsfb_var.vsync_len = mode->vsync_len;
	mxsfb_var.sync = mode->sync;

	panel_info.vl_col = mode->xres;
	panel_info.vl_row = mode->yres;

	return 0;
}
