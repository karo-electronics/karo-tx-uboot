/*
 * Freescale GPMI NFC NAND Flash Driver
 *
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
 * Copyright (C) 2008 Embedded Alley Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <malloc.h>
#include <watchdog.h>
#include <linux/err.h>
#include <linux/mtd/compat.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#include <asm/sizes.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/dma-mapping.h>
#include <asm/arch/mx28.h>

#ifdef CONFIG_JFFS2_NAND
#include <jffs2/jffs2.h>
#endif

#include <asm/arch/regs-clkctrl.h>
#include <asm/arch/mxs_gpmi-regs.h>
#include <asm/arch/mxs_gpmi-bch-regs.h>

#ifdef _DEBUG
static int debug = 1;
#define dbg_lvl(l)		(debug > (l))
#define DBG(l, fmt...)		do { if (dbg_lvl(l)) printk(KERN_DEBUG fmt); } while (0)
#else
#define dbg_lvl(n)		0
#define DBG(l, fmt...)		do { } while (0)
#endif

#define dma_timeout		1000
#define create_bbt		1

#define MAX_CHIP_COUNT		CONFIG_SYS_MAX_NAND_DEVICE
#define COMMAND_BUFFER_SIZE	10
#define MAX_PIO_WORDS		16

/* dmaengine interface */
enum dma_status {
	DMA_SUCCESS,
	DMA_IN_PROGRESS,
	DMA_PAUSED,
	DMA_ERROR,
};

/* MXS APBH DMA controller interface */
#define HW_APBHX_CTRL0				0x000
#define BM_APBH_CTRL0_APB_BURST8_EN		(1 << 29)
#define BM_APBH_CTRL0_APB_BURST_EN		(1 << 28)
#define BP_APBH_CTRL0_CLKGATE_CHANNEL		8
#define BP_APBH_CTRL0_RESET_CHANNEL		16
#define HW_APBHX_CTRL1				0x010
#define HW_APBHX_CTRL2				0x020
#define HW_APBHX_CHANNEL_CTRL			0x030
#define BP_APBHX_CHANNEL_CTRL_RESET_CHANNEL	16
#define HW_APBH_VERSION				(cpu_is_mx23() ? 0x3f0 : 0x800)
#define HW_APBX_VERSION				0x800
#define BP_APBHX_VERSION_MAJOR			24
#define HW_APBHX_CHn_NXTCMDAR(n)		(0x110 + (n) * 0x70)
#define HW_APBHX_CHn_SEMA(n)			(0x140 + (n) * 0x70)

/*
 * ccw bits definitions
 *
 * COMMAND:		0..1	(2)
 * CHAIN:		2	(1)
 * IRQ:			3	(1)
 * NAND_LOCK:		4	(1) - not implemented
 * NAND_WAIT4READY:	5	(1) - not implemented
 * DEC_SEM:		6	(1)
 * WAIT4END:		7	(1)
 * HALT_ON_TERMINATE:	8	(1)
 * TERMINATE_FLUSH:	9	(1)
 * RESERVED:		10..11	(2)
 * PIO_NUM:		12..15	(4)
 */
#define BP_CCW_COMMAND		0
#define BM_CCW_COMMAND		(3 << 0)
#define CCW_CHAIN		(1 << 2)
#define CCW_IRQ			(1 << 3)
#define CCW_DEC_SEM		(1 << 6)
#define CCW_WAIT4END		(1 << 7)
#define CCW_HALT_ON_TERM	(1 << 8)
#define CCW_TERM_FLUSH		(1 << 9)
#define BP_CCW_PIO_NUM		12
#define BM_CCW_PIO_NUM		(0xf << 12)

#define BF_CCW(value, field)	(((value) << BP_CCW_##field) & BM_CCW_##field)

#define MXS_DMA_CMD_NO_XFER	0
#define MXS_DMA_CMD_WRITE	1
#define MXS_DMA_CMD_READ	2
#define MXS_DMA_CMD_DMA_SENSE	3

struct mxs_dma_ccw {
	u32		next;
	u16		bits;
	u16		xfer_bytes;
#define MAX_XFER_BYTES	0xff00
	u32		bufaddr;
#define MXS_PIO_WORDS	16
	u32		pio_words[MXS_PIO_WORDS];
};

#define NUM_CCW	(int)(PAGE_SIZE / sizeof(struct mxs_dma_ccw))

struct mxs_dma_chan {
	struct mxs_dma_engine		*mxs_dma;
	int				chan_id;
	struct mxs_dma_ccw		*ccw;
	int				ccw_idx;
	unsigned long			ccw_phys;
	enum dma_status			status;
#define MXS_DMA_SG_LOOP			(1 << 0)
};

#define MXS_DMA_CHANNELS		16
#define MXS_DMA_CHANNELS_MASK		0xffff

struct mxs_dma_engine {
	int				dev_id;
	void __iomem			*base;
	struct mxs_dma_chan		mxs_chans[MAX_CHIP_COUNT];
};

struct mxs_gpmi {
	struct mxs_dma_engine mxs_dma;
	void __iomem *gpmi_regs;
	void __iomem *bch_regs;
	void __iomem *dma_regs;

	unsigned int chip_count;

	struct mtd_partition *parts;
	unsigned int nr_parts;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	struct nand_ecclayout ecc_layout;

	int current_chip;

	void *page_buf;
	void *oob_buf;
	void *data_buf;

	int command_length;
	u32 pio_data[MAX_PIO_WORDS];
	u8 cmd_buf[COMMAND_BUFFER_SIZE];

	unsigned block0_ecc_strength:5,
		blockn_ecc_strength:5,
		swap_block_mark:1,
		block_mark_bit_offset:3,
		block_mark_byte_offset:14;
};

static uint8_t scan_ff_pattern[] = { 0xff };
static struct nand_bbt_descr gpmi_bbt_descr = {
	.options	= 0,
	.offs		= 0,
	.len		= 1,
	.pattern	= scan_ff_pattern
};

static uint8_t bbt_pattern[] = {'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr mxs_gpmi_bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_2BIT |
		NAND_BBT_VERSION | NAND_BBT_PERCHIP |
		NAND_BBT_NO_OOB,
	.offs =	0,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr mxs_gpmi_bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_2BIT |
		NAND_BBT_VERSION | NAND_BBT_PERCHIP |
		NAND_BBT_NO_OOB,
	.offs =	0,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = mirror_pattern,
};

/* MXS DMA implementation */
#ifdef _DEBUG
static inline u32 __mxs_dma_readl(struct mxs_dma_engine *mxs_dma,
					unsigned int reg,
					const char *name, const char *fn, int ln)
{
	u32 val;
	void __iomem *addr = mxs_dma->base + reg;

	val = readl(addr);
	DBG(3, "%s@%d: Read %08x from %s[%08x]\n", fn, ln, val, name,
		APBHDMA_BASE_ADDR + reg);
	return val;
}
#define mxs_dma_readl(t, r)		__mxs_dma_readl(t, r, #r, __func__, __LINE__)

static inline void __mxs_dma_writel(u32 val,
				struct mxs_dma_engine *mxs_dma, unsigned int reg,
				const char *name, const char *fn, int ln)
{
	void __iomem *addr = mxs_dma->base + reg;

	DBG(3, "%s@%d: Writing %08x to %s[%08x]\n", fn, ln, val, name,
		APBHDMA_BASE_ADDR + reg);
	writel(val, addr);
}
#define mxs_dma_writel(v, t, r)		__mxs_dma_writel(v, t, r, #r, __func__, __LINE__)
#else
static inline u32 mxs_dma_readl(struct mxs_dma_engine *mxs_dma,
				unsigned int reg)
{
	BUG_ON(mxs_dma->base == NULL);
	return readl(mxs_dma->base + reg);
}

static inline void mxs_dma_writel(u32 val, struct mxs_dma_engine *mxs_dma,
				unsigned int reg)
{
	BUG_ON(mxs_dma->base == NULL);
	writel(val, mxs_dma->base + reg);
}
#endif

static inline void mxs_dma_clkgate(struct mxs_dma_chan *mxs_chan, int enable)
{
	struct mxs_dma_engine *mxs_dma = mxs_chan->mxs_dma;
	int chan_id = mxs_chan->chan_id;
	int set_clr = enable ? MXS_CLR_ADDR : MXS_SET_ADDR;

	/* enable apbh channel clock */
	mxs_dma_writel(1 << chan_id,
		mxs_dma, HW_APBHX_CTRL0 + set_clr);
}

static void mxs_dma_reset_chan(struct mxs_dma_chan *mxs_chan)
{
	struct mxs_dma_engine *mxs_dma = mxs_chan->mxs_dma;
	int chan_id = mxs_chan->chan_id;

	mxs_dma_writel(1 << (chan_id + BP_APBHX_CHANNEL_CTRL_RESET_CHANNEL),
		mxs_dma, HW_APBHX_CHANNEL_CTRL + MXS_SET_ADDR);
}

static void mxs_dma_enable_chan(struct mxs_dma_chan *mxs_chan)
{
	struct mxs_dma_engine *mxs_dma = mxs_chan->mxs_dma;
	int chan_id = mxs_chan->chan_id;

	/* clkgate needs to be enabled before writing other registers */
	mxs_dma_clkgate(mxs_chan, 1);

	/* set cmd_addr up */
	mxs_dma_writel(mxs_chan->ccw_phys,
		mxs_dma, HW_APBHX_CHn_NXTCMDAR(chan_id));

	/* write 1 to SEMA to kick off the channel */
	mxs_dma_writel(1, mxs_dma, HW_APBHX_CHn_SEMA(chan_id));
}

static void mxs_dma_disable_chan(struct mxs_dma_chan *mxs_chan)
{
	/* disable apbh channel clock */
	mxs_dma_clkgate(mxs_chan, 0);

	mxs_chan->status = DMA_SUCCESS;
}

#ifdef _DEBUG
static inline u32 __mxs_gpmi_readl(struct mxs_gpmi *gpmi,
					unsigned int reg,
					const char *name, const char *fn, int ln)
{
	u32 val;
	void __iomem *addr = gpmi->gpmi_regs + reg;

	val = readl(addr);
	DBG(3, "%s@%d: Read %08x from %s[%08x]\n", fn, ln, val, name,
		GPMI_BASE_ADDR + reg);
	return val;
}
#define mxs_gpmi_readl(t, r)		__mxs_gpmi_readl(t, r, #r, __func__, __LINE__)

static inline void __mxs_gpmi_writel(u32 val,
				struct mxs_gpmi *gpmi, unsigned int reg,
				const char *name, const char *fn, int ln)
{
	void __iomem *addr = gpmi->gpmi_regs + reg;

	DBG(3, "%s@%d: Writing %08x to %s[%08x]\n", fn, ln, val, name,
		GPMI_BASE_ADDR + reg);
	writel(val, addr);
}
#define mxs_gpmi_writel(v, t, r)	__mxs_gpmi_writel(v, t, r, #r, __func__, __LINE__)

static inline u32 __mxs_bch_readl(struct mxs_gpmi *gpmi,
					unsigned int reg, const char *name,
					const char *fn, int ln)
{
	u32 val;
	void __iomem *addr = gpmi->bch_regs + reg;

	val = readl(addr);
	DBG(3, "%s@%d: Read %08x from %s[%08x]\n", fn, ln, val, name,
		BCH_BASE_ADDR + reg);
	return val;
}
#define mxs_bch_readl(t, r)		__mxs_bch_readl(t, r, #r, __func__, __LINE__)

static inline void __mxs_bch_writel(u32 val,
				struct mxs_gpmi *gpmi, unsigned int reg,
				const char *name, const char *fn, int ln)
{
	void __iomem *addr = gpmi->bch_regs + reg;

	DBG(3, "%s@%d: Writing %08x to %s[%08x]\n", fn, ln, val, name,
		BCH_BASE_ADDR + reg);
	writel(val, addr);
}
#define mxs_bch_writel(v, t, r)		__mxs_bch_writel(v, t, r, #r, __func__, __LINE__)
#else
static inline u32 mxs_gpmi_readl(struct mxs_gpmi *gpmi, unsigned int reg)
{
	return readl(gpmi->gpmi_regs + reg);
}

static inline void mxs_gpmi_writel(u32 val,
				struct mxs_gpmi *gpmi, unsigned int reg)
{
	writel(val, gpmi->gpmi_regs + reg);
}

static inline u32 mxs_bch_readl(struct mxs_gpmi *gpmi, unsigned int reg)
{
	return readl(gpmi->bch_regs + reg);
}

static inline void mxs_bch_writel(u32 val,
				struct mxs_gpmi *gpmi, unsigned int reg)
{
	writel(val, gpmi->bch_regs + reg);
}
#endif

static inline struct mxs_dma_chan *mxs_gpmi_dma_chan(struct mxs_gpmi *gpmi,
						unsigned int dma_channel)
{
	BUG_ON(dma_channel >= ARRAY_SIZE(gpmi->mxs_dma.mxs_chans));
	DBG(3, "%s: DMA chan[%d]=%p[%d]\n", __func__,
		dma_channel, &gpmi->mxs_dma.mxs_chans[dma_channel],
		gpmi->mxs_dma.mxs_chans[dma_channel].chan_id);
	return &gpmi->mxs_dma.mxs_chans[dma_channel];
}

#define DUMP_DMA_CONTEXT

static int first = 1;
#ifdef DUMP_DMA_CONTEXT

#define APBH_DMA_PHYS_ADDR	(MXS_IO_BASE_ADDR + 0x004000)

static void dump_dma_context(struct mxs_gpmi *gpmi, const char *title)
{
	void *q;
	u32 *p;
	unsigned int i;

	if (!dbg_lvl(3))
		return;

	DBG(0, "%s: %s\n", __func__, title);

	DBG(0, "%s: GPMI:\n", __func__);
	{
		void __iomem *GPMI = gpmi->gpmi_regs;
		static u32 old[13];

		p = q = GPMI;

		for (i = 0; i < ARRAY_SIZE(old); i++) {
			u32 val = readl(gpmi->gpmi_regs + i * 0x10);

			if (first || val != old[i]) {
				if (first)
					DBG(0, "    [%p] 0x%08x\n",
						p, val);
				else
					DBG(0, "    [%p] 0x%08x -> 0x%08x\n",
						p, old[i], val);
				old[i] = val;
			}
			q += 0x10;
			p = q;
		}
	}

	DBG(0, "%s: BCH:\n", __func__);
	{
		void *BCH = gpmi->bch_regs;
		static u32 old[22];

		p = q = BCH;

		for (i = 0; i < ARRAY_SIZE(old); i++) {
			u32 val = readl(gpmi->bch_regs + i * 0x10);

			if (first || val != old[i]) {
				if (first)
					DBG(0, "    [%p] 0x%08x\n",
						q, val);
				else
					DBG(0, "    [%p] 0x%08x -> 0x%08x\n",
						q, old[i], val);
				old[i] = val;
			}
			q += 0x10;
			p = q;
		}
	}

	DBG(0, "%s: APBH:\n", __func__);
	{
		void *APBH = gpmi->dma_regs;
		static u32 old[7];
		static u32 chan[16][7];

		p = q = APBH;

		for (i = 0; i < ARRAY_SIZE(old); i++) {
			u32 val = readl(gpmi->dma_regs + i * 0x10);

			if (first || val != old[i]) {
				if (first)
					DBG(0, "    [%p] 0x%08x\n",
						q, val);
				else
					DBG(0, "    [%p] 0x%08x -> 0x%08x\n",
						q, old[i], val);
				old[i] = val;
			}
			q += 0x10;
			p = q;
		}
		for (i = 0; i < ARRAY_SIZE(chan); i++) {
			int j;

			printk("CHAN %2d:\n", i);
			for (j = 0; j < ARRAY_SIZE(chan[i]); j++) {
				u32 val;

				p = q = APBH + 0x100 + i * 0x70 + j * 0x10;

				val = readl(gpmi->dma_regs + 0x100 + i * 0x70 + j * 0x10);

				if (first || val != chan[i][j]) {
					if (first)
						DBG(0, "    [%p] 0x%08x\n",
							q, val);
					else
						DBG(0, "    [%p] 0x%08x -> 0x%08x\n",
							q, chan[i][j], val);
					chan[i][j] = val;
				}
				q += 0x10;
				p = q;
			}
		}
	}
	first = 0;
}
#else
static inline void dump_dma_context(struct mxs_gpmi *gpmi, char *title)
{
}
#endif

static int mxs_gpmi_init_hw(struct mxs_gpmi *gpmi)
{
	dump_dma_context(gpmi, "BEFORE INIT");

	if (mxs_gpmi_readl(gpmi, HW_GPMI_CTRL0) & 0xc0000000) {
		DBG(0, "%s: Resetting GPMI\n", __func__);
		mxs_reset_block(gpmi->gpmi_regs);
	}

	mxs_gpmi_writel(BM_GPMI_CTRL1_GPMI_MODE, gpmi, HW_GPMI_CTRL1_CLR);
	mxs_gpmi_writel(BM_GPMI_CTRL1_ATA_IRQRDY_POLARITY,
			gpmi, HW_GPMI_CTRL1_SET);

	/* Disable write protection and Select BCH ECC */
	mxs_gpmi_writel(BM_GPMI_CTRL1_DEV_RESET | BM_GPMI_CTRL1_BCH_MODE,
			gpmi, HW_GPMI_CTRL1_SET);

	/* Select BCH ECC. */
	mxs_gpmi_writel(BM_GPMI_CTRL1_BCH_MODE,
			gpmi, HW_GPMI_CTRL1_SET);

	dump_dma_context(gpmi, "AFTER INIT");
	return 0;
}

static int mxs_dma_chan_init(struct mxs_dma_chan *mxs_chan, int chan_id)
{
	mxs_chan->ccw = dma_alloc_coherent(PAGE_SIZE, &mxs_chan->ccw_phys);
	if (mxs_chan->ccw == NULL)
		return -ENOMEM;

	DBG(0, "%s: mxs_chan[%d]=%p ccw=%p\n", __func__,
		chan_id, mxs_chan, mxs_chan->ccw);

	memset(mxs_chan->ccw, 0, PAGE_SIZE);
	mxs_chan->chan_id = chan_id;

	mxs_dma_clkgate(mxs_chan, 1);
	mxs_dma_reset_chan(mxs_chan);
	mxs_dma_clkgate(mxs_chan, 0);
	return 0;
}

static int mxs_dma_init(struct mxs_gpmi *gpmi, int dma_chan,
			int num_dma_channels)
{
	int ret;
	struct mxs_dma_engine *mxs_dma = &gpmi->mxs_dma;
	int i;

	writel(readl(CLKCTRL_BASE_ADDR + HW_CLKCTRL_GPMI) & ~BM_CLKCTRL_GPMI_CLKGATE,
		CLKCTRL_BASE_ADDR + HW_CLKCTRL_GPMI);

	mxs_dma->base = gpmi->dma_regs;

	ret = mxs_reset_block(mxs_dma->base);
	if (ret) {
		printk(KERN_ERR "%s: Failed to reset APBH DMA controller\n", __func__);
		return ret;
	}

	mxs_dma_writel(BM_APBH_CTRL0_APB_BURST_EN,
		mxs_dma, HW_APBHX_CTRL0 + MXS_SET_ADDR);
	mxs_dma_writel(BM_APBH_CTRL0_APB_BURST8_EN,
		mxs_dma, HW_APBHX_CTRL0 + MXS_SET_ADDR);

	for (i = 0; i < num_dma_channels; i++) {
		struct mxs_dma_chan *mxs_chan = &mxs_dma->mxs_chans[i];

		mxs_chan->mxs_dma = mxs_dma;
		ret = mxs_dma_chan_init(mxs_chan, dma_chan + i);
		if (ret)
			return ret;
	}
	return 0;
}

static int mxs_dma_prep_slave(struct mxs_dma_chan *mxs_chan, void *buffer,
				dma_addr_t buf_dma, int len,
				enum dma_data_direction direction,
				int append)
{
	int ret;
	int idx = append ? mxs_chan->ccw_idx : 0;
	struct mxs_dma_ccw *ccw;

	DBG(1, "%s: mxs_chan=%p status=%d append=%d ccw=%p\n", __func__,
		mxs_chan, mxs_chan->status, append, mxs_chan->ccw);

	BUG_ON(mxs_chan->ccw == NULL);
	BUG_ON(mxs_chan == NULL);

	if (mxs_chan->status == DMA_IN_PROGRESS && !append)
		return -EBUSY;

	if (mxs_chan->status != DMA_IN_PROGRESS && append) {
		ret = -EINVAL;
		goto err_out;
	}

	if (idx >= NUM_CCW) {
		printk(KERN_ERR "maximum number of segments exceeded: %d > %d\n",
			idx, NUM_CCW);
		ret = -EINVAL;
		goto err_out;
	}

	mxs_chan->status = DMA_IN_PROGRESS;

	if (append) {
		BUG_ON(idx < 1);
		ccw = &mxs_chan->ccw[idx - 1];
		ccw->next = mxs_chan->ccw_phys + sizeof(*ccw) * idx;
		ccw->bits |= CCW_CHAIN;
		ccw->bits &= ~CCW_IRQ;
		ccw->bits &= ~CCW_DEC_SEM;
		ccw->bits &= ~CCW_WAIT4END;

		DBG(3, "%s: Appending sg to list[%d]@%p: next=0x%08x bits=0x%08x\n",
			__func__, idx, ccw, ccw->next, ccw->bits);
	} else {
		idx = 0;
	}
	ccw = &mxs_chan->ccw[idx++];
	if (direction == DMA_NONE) {
		int j;
		u32 *pio = buffer;

		for (j = 0; j < len; j++)
			ccw->pio_words[j] = *pio++;

		if (dbg_lvl(3)) {
			DBG(0, "%s: Storing %d PIO words in ccw[%d]@%p:", __func__,
				len, idx - 1, ccw);
			for (j = 0; j < len; j++) {
				printk(" %08x", ccw->pio_words[j]);
			}
			printk("\n");
		}
		ccw->bits = 0;
		ccw->bits |= CCW_IRQ;
		ccw->bits |= CCW_DEC_SEM;
		ccw->bits |= CCW_WAIT4END;
		ccw->bits |= CCW_HALT_ON_TERM;
		ccw->bits |= CCW_TERM_FLUSH;
		ccw->bits |= BF_CCW(len, PIO_NUM);
		ccw->bits |= BF_CCW(MXS_DMA_CMD_NO_XFER, COMMAND);
	} else {
		if (len > MAX_XFER_BYTES) {
			printk(KERN_ERR "maximum bytes for sg entry exceeded: %d > %d\n",
				len, MAX_XFER_BYTES);
			ret = -EINVAL;
			goto err_out;
		}

		ccw->next = mxs_chan->ccw_phys + sizeof(*ccw) * idx;
		ccw->bufaddr = buf_dma;
		ccw->xfer_bytes = len;

		ccw->bits = 0;
		ccw->bits |= CCW_CHAIN;
		ccw->bits |= CCW_HALT_ON_TERM;
		ccw->bits |= CCW_TERM_FLUSH;
		ccw->bits |= BF_CCW(direction == DMA_FROM_DEVICE ?
				MXS_DMA_CMD_WRITE : MXS_DMA_CMD_READ,
				COMMAND);

		ccw->bits &= ~CCW_CHAIN;
		ccw->bits |= CCW_IRQ;
		ccw->bits |= CCW_DEC_SEM;
		ccw->bits |= CCW_WAIT4END;
		DBG(3, "%s: DMA descriptor: ccw=%p next=0x%08x bufadr=%08x xfer_bytes=%08x bits=0x%08x\n",
			__func__, ccw, ccw->next, ccw->bufaddr,
			ccw->xfer_bytes, ccw->bits);
	}

	mxs_chan->ccw_idx = idx;

	return 0;

err_out:
	mxs_chan->ccw_idx = 0;
	mxs_chan->status = DMA_ERROR;
	return ret;
}

static int mxs_dma_submit(struct mxs_gpmi *gpmi, struct mxs_dma_chan *mxs_chan)
{
	int ret;
	int first = 1;
	u32 stat1, stat2;
	int chan_id = mxs_chan->chan_id;
	u32 chan_mask = 1 << chan_id;
	struct mxs_dma_engine *mxs_dma = mxs_chan->mxs_dma;
	long timeout = 1000;

	dump_dma_context(gpmi, "BEFORE");

	mxs_dma_enable_chan(mxs_chan);

	dump_dma_context(gpmi, "WITHIN");

	while (1) {
		stat1 = mxs_dma_readl(mxs_dma, HW_APBHX_CTRL1);
		stat2 = mxs_dma_readl(mxs_dma, HW_APBHX_CTRL2);
		if ((stat1 & chan_mask) || (stat2 & chan_mask)) {
			break;
		}
		if (first) {
			DBG(1, "Waiting for DMA channel %d to finish\n",
				chan_id);
			first = 0;
		}
		if (timeout-- < 0)
			return -ETIME;
		udelay(100);
	}

	dump_dma_context(gpmi, "AFTER");

	mxs_dma_writel(chan_mask, mxs_dma, HW_APBHX_CTRL1 + MXS_CLR_ADDR);
	mxs_dma_writel(chan_mask, mxs_dma, HW_APBHX_CTRL2 + MXS_CLR_ADDR);
	stat2 = ((stat2 >> MXS_DMA_CHANNELS) & stat2) |
		(~(stat2 >> MXS_DMA_CHANNELS) & stat2 & ~stat1);
	if (stat2 & chan_mask) {
		printk(KERN_ERR "DMA error in channel %d\n", chan_id);
		ret = -EIO;
	} else if (stat1 & chan_mask) {
		DBG(0, "DMA channel %d finished\n", chan_id);
		ret = 0;
	} else {
		printk(KERN_ERR "DMA channel %d early termination\n", chan_id);
		ret = -EINVAL;
	}
	mxs_chan->status = ret == 0 ? DMA_SUCCESS : DMA_ERROR;
	return ret;
}

static void mxs_dma_terminate_all(struct mxs_dma_chan *mxs_chan)
{
	mxs_dma_disable_chan(mxs_chan);
}

static int poll_bit(void __iomem *addr, unsigned int mask, long timeout)
{
	while (!(readl(addr) & mask)) {
		udelay(1000);
		timeout--;
	}
	return timeout == 0 ? -ETIME : 0;
}

static int mxs_gpmi_dma_go(struct mxs_gpmi *gpmi,
			int wait_for_bch)
{
	int error;
	struct mxs_dma_chan *mxs_chan = mxs_gpmi_dma_chan(gpmi,
							gpmi->current_chip);

	DBG(1, "> %s\n", __func__);

	error = mxs_dma_submit(gpmi, mxs_chan);
	DBG(1, "%s: mxs_dma_submit returned %d\n", __func__,
		error);
	if (error)
		goto err;

	if (wait_for_bch) {
		DBG(1, "%s: Waiting for BCH completion\n", __func__);
		error = poll_bit(gpmi->bch_regs + HW_BCH_CTRL,
				BM_BCH_CTRL_COMPLETE_IRQ,
				dma_timeout);
		DBG(1, "%s: poll_bit returned %d\n", __func__,
			error);
		DBG(1, "%s: BCH status %08x\n", __func__,
			mxs_bch_readl(gpmi, HW_BCH_STATUS0));
		if (mxs_bch_readl(gpmi, HW_BCH_CTRL) & BM_BCH_CTRL_COMPLETE_IRQ) {
			DBG(1, "%s: Clearing BCH IRQ\n", __func__);
			mxs_bch_writel(BM_BCH_CTRL_COMPLETE_IRQ, gpmi, HW_BCH_CTRL_CLR);
		}

		if (error)
			goto err;
	}
out:
	DBG(1, "< %s: %d\n", __func__, error);
	return error;

err:
	{
		struct mxs_dma_chan *mxs_chan = mxs_gpmi_dma_chan(gpmi,
								gpmi->current_chip);
		dump_dma_context(gpmi, "ERROR");
		mxs_dma_terminate_all(mxs_chan);
	}
	goto out;
}

int mxs_gpmi_dma_setup(struct mxs_gpmi *gpmi, void *buffer, int length,
		int pio_words, enum dma_data_direction dir, int append)

{
	int ret;
	struct mxs_dma_chan *mxs_chan;
	dma_addr_t buf_dma;

	mxs_chan = mxs_gpmi_dma_chan(gpmi, gpmi->current_chip);
	if (mxs_chan == NULL)
		return -EINVAL;

	DBG(1, "%s: buffer=%p len=%u pio=%d append=%d\n", __func__,
		buffer, length, pio_words, append);

	if (pio_words) {
		ret = mxs_dma_prep_slave(mxs_chan, gpmi->pio_data, ~0,
					pio_words, DMA_NONE, append);
		if (ret) {
			mxs_dma_terminate_all(mxs_chan);
			printk(KERN_ERR
				"%s: Failed to setup DMA PIO xfer for %d words: %d\n",
				__func__, pio_words, ret);
			return ret;
		}
		if (buffer == NULL)
			return ret;

		append = 1;
	}

#if 0
	if (dir == DMA_FROM_DEVICE)
		memset(buffer, 0x55, length);
#endif
	buf_dma = dma_map_single(buffer, length, dir);

	DBG(1, "%s: buffer=%p dma_addr=%08x\n", __func__, buffer, buf_dma);

	ret = mxs_dma_prep_slave(mxs_chan, buffer, buf_dma, length, dir, append);
	if (ret) {
		mxs_dma_terminate_all(mxs_chan);
		DBG(1, "%s: mxs_dma_prep_slave() returned %d\n",
			__func__, ret);
		dma_unmap_single(buffer, length, dir);
	}
	return ret;
}

static int mxs_gpmi_dma_xfer(struct mxs_gpmi *gpmi,
			void *buffer, int length, int pio_words,
			enum dma_data_direction dir)
{
	int ret;

	ret = mxs_gpmi_dma_setup(gpmi, buffer, length,
				pio_words, dir, 0);

	if (ret) {
		DBG(1, "%s: mxs_gpmi_dma_setup() returned %d\n",
			__func__, ret);
		return ret;
	}

	DBG(1, "%s: starting DMA xfer\n", __func__);
	ret = mxs_gpmi_dma_go(gpmi, 0);

	DBG(1, "%s: DMA xfer done: %d\n", __func__, ret);
	return ret;
}

/* low level accessor functions */
static int mxs_gpmi_read_data(struct mxs_gpmi *gpmi, int cs,
			void *buffer, size_t length)
{
	int ret;
	u32 command_mode;
	u32 address;

	DBG(2, "%s: buf=%p len=%d\n", __func__, buffer, length);

	memset(buffer, 0x44, length);

	command_mode = BV_GPMI_CTRL0_COMMAND_MODE__READ;
	address = BV_GPMI_CTRL0_ADDRESS__NAND_DATA;

	gpmi->pio_data[0] =
		BF_GPMI_CTRL0_COMMAND_MODE(command_mode) |
		BF_GPMI_CTRL0_CS_V1(cs) |
		BM_GPMI_CTRL0_WORD_LENGTH |
		BF_GPMI_CTRL0_ADDRESS(address) |
		BF_GPMI_CTRL0_XFER_COUNT(length);
	gpmi->pio_data[1] = 0;

	ret = mxs_gpmi_dma_xfer(gpmi, buffer, length, 2, DMA_FROM_DEVICE);
	return ret;
}

/* mtd layer interface */
static void mxs_gpmi_select_chip(struct mtd_info *mtd, int cs)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_gpmi *gpmi = chip->priv;

	gpmi->current_chip = cs;
}

static int mxs_gpmi_dev_ready(struct mtd_info *mtd)
{
	int ready;
	struct nand_chip *chip = mtd->priv;
	struct mxs_gpmi *gpmi = chip->priv;
	u32 mask;
	u32 reg;
	int cs = gpmi->current_chip;

	if (cs < 0)
		return 0;

	DBG(1, "> %s\n", __func__);

	mask = BF_GPMI_STAT_READY_BUSY(1 << cs);
	reg = mxs_gpmi_readl(gpmi, HW_GPMI_STAT);

	ready = !!(reg & mask);
	DBG(1, "< %s: %d\n", __func__, ready);
	return ready;
}

static void mxs_gpmi_swap_bb_mark(struct mxs_gpmi *gpmi,
				void *payload, void *auxiliary)
{
	unsigned char *p = payload + gpmi->block_mark_byte_offset;
	unsigned char *a = auxiliary;
	unsigned int bit = gpmi->block_mark_bit_offset;
	unsigned char mask;
	unsigned char from_data;
	unsigned char from_oob;

	/*
	 * Get the byte from the data area that overlays the block mark. Since
	 * the ECC engine applies its own view to the bits in the page, the
	 * physical block mark won't (in general) appear on a byte boundary in
	 * the data.
	 */
	from_data = (p[0] >> bit) | (p[1] << (8 - bit));

	/* Get the byte from the OOB. */
	from_oob = a[0];

	/* Swap them. */
	a[0] = from_data;

	mask = (0x1 << bit) - 1;
	p[0] = (p[0] & mask) | (from_oob << bit);

	mask = ~0 << bit;
	p[1] = (p[1] & mask) | (from_oob >> (8 - bit));
}

static int mxs_gpmi_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			uint8_t *buf)
{
	int ret = -1;
	struct mxs_gpmi *gpmi = chip->priv;
	int cs = gpmi->current_chip;
	u32 command_mode;
	u32 address;
	u32 ecc_command;
	u32 buffer_mask;
	dma_addr_t buf_dma;
	dma_addr_t oob_dma;

	DBG(3, "%s: read page to buffer %p\n", __func__, buf);

	buf_dma = dma_map_single(gpmi->page_buf, mtd->writesize,
				DMA_FROM_DEVICE);

	oob_dma = dma_map_single(gpmi->oob_buf, mtd->oobsize,
				DMA_FROM_DEVICE);

	command_mode = BV_GPMI_CTRL0_COMMAND_MODE__WAIT_FOR_READY;
	address = BV_GPMI_CTRL0_ADDRESS__NAND_DATA;

	gpmi->pio_data[0] =
		BF_GPMI_CTRL0_COMMAND_MODE(command_mode) |
		BF_GPMI_CTRL0_CS_V1(cs) |
		BM_GPMI_CTRL0_WORD_LENGTH |
		BF_GPMI_CTRL0_ADDRESS(address) |
		BF_GPMI_CTRL0_XFER_COUNT(0);
	gpmi->pio_data[1] = 0;

	ret = mxs_gpmi_dma_setup(gpmi, NULL, 0, 2, DMA_NONE, 0);
	if (ret) {
		goto unmap;
	}

	command_mode = BV_GPMI_CTRL0_COMMAND_MODE__READ;
	address      = BV_GPMI_CTRL0_ADDRESS__NAND_DATA;
	ecc_command  = BV_GPMI_ECCCTRL_ECC_CMD__DECODE;
	buffer_mask  = BV_GPMI_ECCCTRL_BUFFER_MASK__BCH_PAGE |
		BV_GPMI_ECCCTRL_BUFFER_MASK__BCH_AUXONLY;

	gpmi->pio_data[0] = BF_GPMI_CTRL0_COMMAND_MODE(command_mode) |
		BM_GPMI_CTRL0_WORD_LENGTH |
		BF_GPMI_CTRL0_CS_V1(cs) |
		BF_GPMI_CTRL0_ADDRESS(address) |
		BF_GPMI_CTRL0_XFER_COUNT(mtd->writesize + mtd->oobsize);

	gpmi->pio_data[1] = 0;

	gpmi->pio_data[2] = BM_GPMI_ECCCTRL_ENABLE_ECC |
		BF_GPMI_ECCCTRL_ECC_CMD(ecc_command) |
		BF_GPMI_ECCCTRL_BUFFER_MASK(buffer_mask);

	gpmi->pio_data[3] = mtd->writesize + mtd->oobsize;
	gpmi->pio_data[4] = buf_dma;
	gpmi->pio_data[5] = oob_dma;

	ret = mxs_gpmi_dma_setup(gpmi, NULL, 0, 6, DMA_NONE, 1);
	if (ret) {
		goto unmap;
	}

	command_mode = BV_GPMI_CTRL0_COMMAND_MODE__WAIT_FOR_READY;
	address      = BV_GPMI_CTRL0_ADDRESS__NAND_DATA;

	gpmi->pio_data[0] =
		BF_GPMI_CTRL0_COMMAND_MODE(command_mode) |
		BM_GPMI_CTRL0_WORD_LENGTH |
		BF_GPMI_CTRL0_CS_V1(cs) |
		BF_GPMI_CTRL0_ADDRESS(address) |
		BF_GPMI_CTRL0_XFER_COUNT(mtd->writesize + mtd->oobsize);

	gpmi->pio_data[1] = 0;

	ret = mxs_gpmi_dma_setup(gpmi, NULL, 0, 2, DMA_NONE, 1);
	if (ret == 0) {
		ret = mxs_gpmi_dma_go(gpmi, 1);
	}
unmap:
	dma_unmap_single(gpmi->oob_buf, mtd->oobsize,
			DMA_FROM_DEVICE);
	dma_unmap_single(gpmi->page_buf, mtd->writesize,
			DMA_FROM_DEVICE);
	{
#define STATUS_GOOD		0x00
#define STATUS_ERASED		0xff
#define STATUS_UNCORRECTABLE	0xfe
		/* Loop over status bytes, accumulating ECC status. */
		struct nand_chip *chip = mtd->priv;
		int failed = 0;
		int corrected = 0;
		u8 *status = gpmi->oob_buf + mtd->oobavail;
		int i;

		for (i = 0; i < mtd->writesize / chip->ecc.size; i++, status++) {
			if ((*status == STATUS_GOOD) || (*status == STATUS_ERASED))
				continue;

			if (*status == STATUS_UNCORRECTABLE) {
				failed++;
				continue;
			}
			corrected += *status;
		}
		/*
		 * Propagate ECC status to the owning MTD only when failed or
		 * corrected times nearly reaches our ECC correction threshold.
		 */
		if (failed || corrected >= (chip->ecc.size - 1)) {
			DBG(0, "%s: ECC failures: %d\n", __func__, failed);
			mtd->ecc_stats.failed += failed;
			mtd->ecc_stats.corrected += corrected;
		}
	}
	if (ret == 0) {
		if (gpmi->swap_block_mark)
			mxs_gpmi_swap_bb_mark(gpmi, gpmi->page_buf, gpmi->oob_buf);
		if (buf) {
			memcpy(buf, gpmi->page_buf, mtd->writesize);
		}
	} else {
		printk(KERN_ERR "%s: FAILED to read page to buffer %p\n", __func__, buf);
	}
	return ret;
}

static int mxs_gpmi_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			int page, int sndcmd)
{
	DBG(3, "%s: reading OOB of page %d\n", __func__, page);

	memset(chip->oob_poi, dbg_lvl(0) ? 0xfe : 0xff, mtd->oobsize);
	chip->cmdfunc(mtd, NAND_CMD_READ0, 0, page);
	chip->read_buf(mtd, chip->oob_poi, mtd->oobavail);
	return 1;
}

static void mxs_gpmi_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf)
{
	int ret = -1;
	struct mxs_gpmi *gpmi = chip->priv;
	int cs = gpmi->current_chip;
	u32 command_mode;
	u32 address;
	u32 ecc_command;
	u32 buffer_mask;
	dma_addr_t buf_dma;
	dma_addr_t oob_dma;

	DBG(3, "%s: Writing buffer %p\n", __func__, buf);

	memset(gpmi->oob_buf + mtd->oobavail, dbg_lvl(0) ? 0xef : 0xff,
		mtd->oobsize - mtd->oobavail);

	if (buf)
		memcpy(gpmi->page_buf, buf, mtd->writesize);

	if (gpmi->swap_block_mark)
		mxs_gpmi_swap_bb_mark(gpmi, gpmi->page_buf, gpmi->oob_buf);

	buf_dma = dma_map_single(gpmi->page_buf, mtd->writesize,
				DMA_TO_DEVICE);

	oob_dma = dma_map_single(gpmi->oob_buf, mtd->oobsize,
				DMA_TO_DEVICE);

	command_mode = BV_GPMI_CTRL0_COMMAND_MODE__WRITE;
	address      = BV_GPMI_CTRL0_ADDRESS__NAND_DATA;
	ecc_command  = BV_GPMI_ECCCTRL_ECC_CMD__ENCODE;
	buffer_mask  = BV_GPMI_ECCCTRL_BUFFER_MASK__BCH_PAGE |
				BV_GPMI_ECCCTRL_BUFFER_MASK__BCH_AUXONLY;

	gpmi->pio_data[0] =
		BF_GPMI_CTRL0_COMMAND_MODE(command_mode) |
		BM_GPMI_CTRL0_WORD_LENGTH |
		BF_GPMI_CTRL0_CS_V1(cs) |
		BF_GPMI_CTRL0_ADDRESS(address) |
		BF_GPMI_CTRL0_XFER_COUNT(0);
	gpmi->pio_data[1] = 0;
	gpmi->pio_data[2] =
		BM_GPMI_ECCCTRL_ENABLE_ECC |
		BF_GPMI_ECCCTRL_ECC_CMD(ecc_command) |
		BF_GPMI_ECCCTRL_BUFFER_MASK(buffer_mask);
	gpmi->pio_data[3] = mtd->writesize + mtd->oobsize;
	gpmi->pio_data[4] = buf_dma;
	gpmi->pio_data[5] = oob_dma;

	ret = mxs_gpmi_dma_setup(gpmi, NULL, 0, 6, DMA_NONE, 0);
	if (ret == 0) {
		ret = mxs_gpmi_dma_go(gpmi, 1);
	}

	dma_unmap_single(gpmi->oob_buf, mtd->oobsize,
			DMA_TO_DEVICE);
	dma_unmap_single(gpmi->page_buf, mtd->writesize,
			DMA_TO_DEVICE);
	if (ret) {
		printk(KERN_ERR "%s: FAILED!\n", __func__);
	}
}

static int mxs_gpmi_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
			int page)
{
	return -EINVAL;
}

#if 0
static void mxs_gpmi_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf)
{
	memcpy(gpmi->page_buf, buf, mtd->writesize);

}
#endif

static void mxs_gpmi_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_gpmi *gpmi = chip->priv;
	void *xfer_buf;
	int ret = 0;

	if (len > mtd->writesize + mtd->oobsize) {
		DBG(0, "%s: Allocating temporary buffer\n", __func__);
		xfer_buf = kzalloc(len, GFP_KERNEL);
		if (xfer_buf == NULL) {
			printk(KERN_ERR
				"Failed to allocate %u byte for xfer buffer\n",
				len);
			memset(buf, 0xee, len);
		}
	} else {
		xfer_buf = buf;
	}

	DBG(3, "%s: reading %u byte to %p(%p)\n", __func__,
		len, buf, xfer_buf);

	ret = mxs_gpmi_read_data(gpmi, gpmi->current_chip, xfer_buf, len);
	if (xfer_buf != buf) {
		if (ret == 0) {
			memcpy(buf, xfer_buf, len);
		}
		kfree(xfer_buf);
	}
	DBG(1, "< %s %d\n", __func__, ret);
}

static u_char mxs_gpmi_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_gpmi *gpmi = chip->priv;
	u_char *buf = (u_char *)gpmi->pio_data;

	mxs_gpmi_read_buf(mtd, buf, 1);
	return *buf;
}

static void mxs_gpmi_write_buf(struct mtd_info *mtd, const u_char *buf,
			int len)
{
	int ret;
	struct nand_chip *chip = mtd->priv;
	struct mxs_gpmi *gpmi = chip->priv;
	void *xfer_buf = (void *)buf; /* cast away the 'const' */
#if 1
	u32 command_mode;
	u32 address;
	int cs = gpmi->current_chip;

	DBG(3, "%s: writing %u byte from %p\n", __func__, len, buf);

	command_mode = BV_GPMI_CTRL0_COMMAND_MODE__WRITE;
	address = BV_GPMI_CTRL0_ADDRESS__NAND_DATA;

	gpmi->pio_data[0] =
		BF_GPMI_CTRL0_COMMAND_MODE(command_mode) |
		BF_GPMI_CTRL0_CS_V1(cs) |
		BM_GPMI_CTRL0_WORD_LENGTH |
		BF_GPMI_CTRL0_ADDRESS(address) |
		BF_GPMI_CTRL0_XFER_COUNT(len);
	gpmi->pio_data[1] = 0;

	ret = mxs_gpmi_dma_xfer(gpmi, xfer_buf, len, 2, DMA_TO_DEVICE);
#else
	ret = mxs_gpmi_send_data(gpmi, gpmi->current_chip, xfer_buf, len);
#endif
	if (ret)
		printk(KERN_ERR "%s: Failed to write %u byte from %p\n", __func__,
			len, buf);
}

static int mxs_gpmi_scan_bbt(struct mtd_info *mtd)
{
	int ret;

	DBG(0, "%s: \n", __func__);
	ret = nand_scan_bbt(mtd, create_bbt ? &gpmi_bbt_descr : NULL);
	DBG(0, "%s: nand_scan_bbt() returned %d\n", __func__, ret);
	return ret;
}

static int mxs_gpmi_send_command(struct mxs_gpmi *gpmi, unsigned cs,
			void *buffer, unsigned int length)
{
	int error;
	u32 command_mode;
	u32 address;

	DBG(1, "%s: Sending NAND command\n", __func__);

	command_mode = BV_GPMI_CTRL0_COMMAND_MODE__WRITE;
	address = BV_GPMI_CTRL0_ADDRESS__NAND_CLE;

	gpmi->pio_data[0] =
		BF_GPMI_CTRL0_COMMAND_MODE(command_mode) |
		BF_GPMI_CTRL0_CS_V1(cs) |
		BM_GPMI_CTRL0_WORD_LENGTH |
		BF_GPMI_CTRL0_ADDRESS(address) |
		BM_GPMI_CTRL0_ADDRESS_INCREMENT |
		BF_GPMI_CTRL0_XFER_COUNT(length);

	gpmi->pio_data[1] = 0;

	gpmi->pio_data[2] = 0;

	error = mxs_gpmi_dma_xfer(gpmi, buffer, length, 3,
				DMA_TO_DEVICE);
	if (error)
		printk(KERN_ERR "[%s] DMA error\n", __func__);

	return error;
}

static void mxs_gpmi_cmdctrl(struct mtd_info *mtd,
			int data, unsigned int ctrl)
{
	int ret;
	struct nand_chip *chip = mtd->priv;
	struct mxs_gpmi *gpmi = chip->priv;
	unsigned int i;

	DBG(2, "%s: data=%04x ctrl=%04x\n", __func__,
		data, ctrl);
	/*
	 * Every operation begins with a command byte and a series of zero or
	 * more address bytes. These are distinguished by either the Address
	 * Latch Enable (ALE) or Command Latch Enable (CLE) signals being
	 * asserted. When MTD is ready to execute the command, it will deassert
	 * both latch enables.
	 *
	 * Rather than run a separate DMA operation for every single byte, we
	 * queue them up and run a single DMA operation for the entire series
	 * of command and data bytes.
	 */
	if ((ctrl & (NAND_ALE | NAND_CLE))) {
		if (data != NAND_CMD_NONE) {
			DBG(3, "%s: Storing cmd byte %02x\n", __func__, data & 0xff);
			gpmi->cmd_buf[gpmi->command_length++] = data;
		}
		return;
	}
	/*
	 * If control arrives here, MTD has deasserted both the ALE and CLE,
	 * which means it's ready to run an operation. Check if we have any
	 * bytes to send.
	 */
	if (gpmi->command_length == 0)
		return;

	DBG(1, "%s: sending command...\n", __func__);
	for (i = 0; i < gpmi->command_length; i++)
		DBG(2, " 0x%02x", gpmi->cmd_buf[i]);
	DBG(2, "\n");

	ret = mxs_gpmi_send_command(gpmi,
			gpmi->current_chip, gpmi->cmd_buf,
			gpmi->command_length);
	if (ret) {
		printk(KERN_ERR "[%s] Chip: %u, Error %d\n",
			__func__, gpmi->current_chip, ret);
	}

	gpmi->command_length = 0;
	DBG(1, "%s: ...Finished\n", __func__);
}

static int mxs_gpmi_set_ecclayout(struct mxs_gpmi *gpmi,
				int page_size, int oob_size)
{
	struct nand_chip *chip = gpmi->chip;
	struct mtd_info *mtd = gpmi->mtd;
	struct nand_ecclayout *layout = &gpmi->ecc_layout;
	const int meta_size = 10;
	const int block0_size = 512;
	const int blockn_size = 512;
	const int fl0_nblocks = (mtd->writesize >> 9) - !!block0_size;
	int i;

	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.size = blockn_size;
	chip->ecc.layout = layout;

	chip->bbt_td = &mxs_gpmi_bbt_main_descr;
	chip->bbt_md = &mxs_gpmi_bbt_mirror_descr;

	if (create_bbt) {
		chip->bbt_td->options |= NAND_BBT_WRITE | NAND_BBT_CREATE;
		chip->bbt_md->options |= NAND_BBT_WRITE | NAND_BBT_CREATE;
	}

	switch (page_size) {
	case 2048:
		/* default GPMI OOB layout */
		layout->eccbytes = 4 * 10 + 9;
		gpmi->block0_ecc_strength = 8;
		gpmi->blockn_ecc_strength = 8;
		break;

	case 4096:
		if (mtd->oobsize == 128) {
			gpmi->block0_ecc_strength = 8;
			gpmi->blockn_ecc_strength = 8;
		} else {
			gpmi->block0_ecc_strength = 16;
			gpmi->blockn_ecc_strength = 16;
		}
		break;

	case 8192:
		gpmi->block0_ecc_strength = 24;
		gpmi->blockn_ecc_strength = 24;
		break;

	default:
		printk(KERN_ERR "unsupported page size: %u\n", page_size);
		return -EINVAL;
	}

	{
		int chunk0_data_size_in_bits = block0_size * 8;
		int chunk0_ecc_size_in_bits  = gpmi->block0_ecc_strength * 13;
		int chunkn_data_size_in_bits = blockn_size * 8;
		int chunkn_ecc_size_in_bits  = gpmi->blockn_ecc_strength * 13;
		int chunkn_total_size_in_bits = chunkn_data_size_in_bits +
			chunkn_ecc_size_in_bits;

		/* Compute the bit offset of the block mark within the physical page. */
		int block_mark_bit_offset = mtd->writesize * 8;

		/* Subtract the metadata bits. */
		block_mark_bit_offset -= meta_size * 8;

		/* if the first block is metadata only,
		 * subtract the number of ecc bits of that block
		 */
		if (block0_size == 0) {
			block_mark_bit_offset -= chunk0_ecc_size_in_bits;
		}
		/*
		 * Compute the chunk number (starting at zero) in which the block mark
		 * appears.
		 */
		int block_mark_chunk_number =
			block_mark_bit_offset / chunkn_total_size_in_bits;

		/*
		 * Compute the bit offset of the block mark within its chunk, and
		 * validate it.
		 */
		int block_mark_chunk_bit_offset = block_mark_bit_offset -
			(block_mark_chunk_number * chunkn_total_size_in_bits);

		if (block_mark_chunk_bit_offset > chunkn_data_size_in_bits) {
			/*
			 * If control arrives here, the block mark actually appears in
			 * the ECC bits of this chunk. This wont' work.
			 */
			printf("Unsupported page geometry (block mark in ECC): %u:%u",
				mtd->writesize, mtd->oobsize);
			return -EINVAL;
		}

		/*
		 * Now that we know the chunk number in which the block mark appears,
		 * we can subtract all the ECC bits that appear before it.
		 */
		block_mark_bit_offset -= block_mark_chunk_number *
			chunkn_ecc_size_in_bits;

		/*
		 * We now know the absolute bit offset of the block mark within the
		 * ECC-based data. We can now compute the byte offset and the bit
		 * offset within the byte.
		 */
		gpmi->block_mark_byte_offset = block_mark_bit_offset / 8;
		gpmi->block_mark_bit_offset = block_mark_bit_offset % 8;

		DBG(0, "NAND geometry:\n");
		DBG(0, "page size   : %5u byte\n", mtd->writesize);
		DBG(0, "oob size    : %5u byte\n", mtd->oobsize);
		DBG(0, "erase size  : %5u byte\n", mtd->erasesize);
		DBG(0, "metadata    : %5u byte\n", meta_size);
		DBG(0, "ECC:\n");
		DBG(0, "chunk0 level: %5u\n", gpmi->block0_ecc_strength);
		DBG(0, "chunk0 data : %5u bit (%5u byte)\n",
			chunk0_data_size_in_bits,
			DIV_ROUND_UP(chunk0_data_size_in_bits, 8));
		DBG(0, "chunk0 ECC  : %5u bit (%5u byte)\n",
			chunk0_ecc_size_in_bits,
			DIV_ROUND_UP(chunk0_ecc_size_in_bits, 8));

		DBG(0, "chunkn level: %5u\n", gpmi->blockn_ecc_strength);
		DBG(0, "chunkn data : %5u bit (%5u byte)\n",
			chunkn_data_size_in_bits,
			DIV_ROUND_UP(chunkn_data_size_in_bits, 8));
		DBG(0, "chunkn ECC  : %5u bit (%5u byte)\n",
			chunkn_ecc_size_in_bits,
			DIV_ROUND_UP(chunkn_ecc_size_in_bits, 8));
		DBG(0, "BB chunk    : %5d\n", block_mark_chunk_number);
		DBG(0, "BB byte offs: %5u\n", gpmi->block_mark_byte_offset);
		DBG(0, "BB bit offs : %5u\n", gpmi->block_mark_bit_offset);
	}

	for (i = 0; i < layout->eccbytes; i++) {
		layout->eccpos[i] = mtd->oobsize - i - 1;
	}
	layout->oobfree[0].length = meta_size;

	chip->ecc.bytes = layout->eccbytes;

	DBG(0, "%s: Resetting BCH\n", __func__);
	mxs_reset_block(gpmi->bch_regs);

	mxs_bch_writel(
		BF_BCH_FLASH0LAYOUT0_NBLOCKS(fl0_nblocks) |
		BF_BCH_FLASH0LAYOUT0_META_SIZE(meta_size) |
		BF_BCH_FLASH0LAYOUT0_ECC0(gpmi->block0_ecc_strength >> 1) |
		BF_BCH_FLASH0LAYOUT0_DATA0_SIZE(block0_size),
		gpmi, HW_BCH_FLASH0LAYOUT0);

	mxs_bch_writel(
		BF_BCH_FLASH0LAYOUT1_PAGE_SIZE(mtd->writesize + mtd->oobsize) |
		BF_BCH_FLASH0LAYOUT1_ECCN(gpmi->blockn_ecc_strength >> 1) |
		BF_BCH_FLASH0LAYOUT1_DATAN_SIZE(blockn_size),
		gpmi, HW_BCH_FLASH0LAYOUT1);

	mxs_bch_writel(0, gpmi, HW_BCH_LAYOUTSELECT);

	mxs_bch_writel(BM_BCH_CTRL_COMPLETE_IRQ_EN, gpmi, HW_BCH_CTRL_SET);
	return 0;
}

int mxs_gpmi_nand_init(struct mtd_info *mtd, struct nand_chip *chip)
{
	int ret;
	struct mxs_gpmi *gpmi;

	gpmi = kzalloc(sizeof(struct mxs_gpmi), GFP_KERNEL);
	if (gpmi == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	gpmi->mtd = mtd;
	gpmi->chip = chip;

	gpmi->chip_count = CONFIG_SYS_MAX_NAND_DEVICE;
	gpmi->swap_block_mark = 1;

	gpmi->gpmi_regs = __ioremap(GPMI_BASE_ADDR, SZ_4K, 1);
	if (gpmi->gpmi_regs == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	gpmi->bch_regs = __ioremap(BCH_BASE_ADDR, SZ_4K, 1);
	if (gpmi->bch_regs == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	gpmi->dma_regs = __ioremap(APBHDMA_BASE_ADDR, SZ_4K, 1);
	if (gpmi->dma_regs == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ret = mxs_dma_init(gpmi, CONFIG_SYS_MXS_DMA_CHANNEL,
			CONFIG_SYS_MAX_NAND_DEVICE);
	if (ret)
		goto out;

	ret = mxs_gpmi_init_hw(gpmi);
	if (ret)
		goto out;

	chip->priv = gpmi;

	chip->select_chip = mxs_gpmi_select_chip;
	chip->cmd_ctrl = mxs_gpmi_cmdctrl;
	chip->dev_ready = mxs_gpmi_dev_ready;

	chip->read_byte = mxs_gpmi_read_byte;
	chip->read_buf = mxs_gpmi_read_buf;
	chip->write_buf = mxs_gpmi_write_buf;

	chip->scan_bbt = mxs_gpmi_scan_bbt;

	chip->options |= NAND_NO_SUBPAGE_WRITE;
	chip->options |= NAND_USE_FLASH_BBT | NAND_USE_FLASH_BBT_NO_OOB;

	chip->ecc.read_page = mxs_gpmi_read_page;
	chip->ecc.read_oob = mxs_gpmi_read_oob;
	chip->ecc.write_page = mxs_gpmi_write_page;
	chip->ecc.write_oob = mxs_gpmi_write_oob;

	DBG(0, "%s: Scanning for NAND chips\n", __func__);
	ret = nand_scan_ident(mtd, gpmi->chip_count);
	if (ret) {
		DBG(0, "%s: Failed to scan for NAND chips\n", __func__);
		goto out;
	}
	DBG(0, "%s: pagesize=%d oobsize=%d\n", __func__,
		mtd->writesize, mtd->oobsize);

	gpmi->page_buf = kzalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);
	if (gpmi->page_buf == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	gpmi->oob_buf = gpmi->page_buf + mtd->writesize;

	ret = mxs_gpmi_set_ecclayout(gpmi, mtd->writesize, mtd->oobsize);
	if (ret) {
		DBG(0, "%s: Unsupported ECC layout\n", __func__);
		return ret;
	}
	DBG(0, "%s: NAND scan succeeded\n", __func__);
	return 0;
out:
	kfree(gpmi);
	return ret;
}
