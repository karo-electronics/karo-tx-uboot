/* SPDX-License-Identifier: GPL-2.0  */
/*
 * Copyright (C) STMicroelectronics 2020 - All Rights Reserved
 * Author: Gabriel Fernandez <gabriel.fernandez@st.com> for STMicroelectronics.
 */

struct stm32_clock_match_data;

struct stm32_mux_cfg {
	const char * const *parent_names;
	u8	num_parents;
	u32	reg_off;
	u8	shift;
	u8	width;
	u8	mux_flags;
	u32	*table;
};

struct stm32_gate_cfg {
	u32	reg_off;
	u8	bit_idx;
	u8	gate_flags;
	u8	set_clr;
};

struct stm32_div_cfg {
	u32	reg_off;
	u8	shift;
	u8	width;
	u8	div_flags;
	const struct clk_div_table *table;
};

struct stm32_composite_cfg {
	int mux;
	int gate;
	int div;
};

struct clock_config {
	unsigned long id;
	const char *name;
	const char *parent_name;
	unsigned long flags;
	int sec_id;
	void *clock_cfg;

	struct clk *(*func)(struct device *dev,
			    const struct stm32_clock_match_data *data,
			    void __iomem *base,
			    spinlock_t *lock,
			    const struct clock_config *cfg);
};

struct stm32_clock_match_data {
	unsigned int			num_clocks;
	const struct clock_config	*tab_clocks;
	unsigned int			maxbinding;
	const struct stm32_gate_cfg	*gates;
	const struct stm32_mux_cfg	*muxes;
	const struct stm32_div_cfg	*dividers;

	int (*check_security)(void __iomem *base,
			      const struct clock_config *cfg);
};

int stm32_rcc_init(struct device *dev,
		   const struct stm32_clock_match_data *data,
		   void __iomem *base);

#define NO_ID 0xFFFF0000

#define NO_STM32_MUX	-1
#define NO_STM32_DIV	-1
#define NO_STM32_GATE	-1

struct clk *
_clk_stm32_gate_register(struct device *dev,
			 const struct stm32_clock_match_data *data,
			 void __iomem *base, spinlock_t *lock,
			 const struct clock_config *cfg);

struct clk *
_clk_stm32_register_composite(struct device *dev,
			      const struct stm32_clock_match_data *data,
			      void __iomem *base, spinlock_t *lock,
			      const struct clock_config *cfg);

struct stm32_clk_gate_cfg {
	int gate_id;
};

#define STM32_GATE(_id, _name, _parent, _flags, _gate_id, _sec_id)\
{\
	.id		= _id,\
	.sec_id		= _sec_id,\
	.name		= _name,\
	.parent_name	= _parent,\
	.flags		= _flags,\
	.clock_cfg	= &(struct stm32_clk_gate_cfg) {\
		.gate_id	= _gate_id,\
	},\
	.func		= _clk_stm32_gate_register,\
}

struct stm32_clk_composite_cfg {
	int	gate_id;
	int	mux_id;
	int	div_id;
};

#define STM32_COMPOSITE(_id, _name, _flags, _sec_id,\
			_gate_id, _mux_id, _div_id)\
{\
	.id		= _id,\
	.name		= _name,\
	.sec_id		= _sec_id,\
	.flags		= _flags,\
	.clock_cfg	= &(struct stm32_clk_composite_cfg) {\
		.gate_id	= _gate_id,\
		.mux_id		= _mux_id,\
		.div_id		= _div_id,\
	},\
	.func		= _clk_stm32_register_composite,\
}

#define STM32_COMPOSITE_NOMUX(_id, _name, _parent, _flags, _sec_id,\
			      _gate_id, _div_id)\
{\
	.id		= _id,\
	.name		= _name,\
	.parent_name	= _parent,\
	.sec_id		= _sec_id,\
	.flags		= _flags,\
	.clock_cfg	= &(struct stm32_clk_composite_cfg) {\
		.gate_id	= _gate_id,\
		.mux_id		= NO_STM32_MUX,\
		.div_id		= _div_id,\
	},\
	.func		= _clk_stm32_register_composite,\
}

extern const struct clk_ops stm32_clk_ops;
extern const struct clk_ops clk_stm32_setclr_gate_ops;
ulong clk_stm32_get_rate_by_name(const char *name);
int clk_stm32_get_by_name(const char *name, struct clk **clkp);
struct clk *clk_stm32_register_setclr_gate(struct device *dev,
					   const char *name,
					   const char *parent_name,
					   unsigned long flags,
					   void __iomem *reg, u8 bit_idx,
					   u8 clk_gate_flags, spinlock_t *lock);

