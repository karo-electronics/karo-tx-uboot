/*
 * Copyright (C) 2013 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* timing parameters for Samsung K9F1G08U0D */
#define tCS_NAND		20
#define tCH_NAND		5
#define tCEA_NAND		25
#define tREA_NAND		20
#define tWP_NAND		15
#define tRP_NAND		15
#define tWC_NAND		30
#define tRC_NAND		30
#define tALS_NAND		15
#define tADL_NAND		100
#define tRHW_NAND		100

/* Values for GPMC_CONFIG1 - signal control parameters */
#define WRAPBURST			(1 << 31)
#define READMULTIPLE			(1 << 30)
#define READTYPE			(1 << 29)
#define WRITEMULTIPLE			(1 << 28)
#define WRITETYPE			(1 << 27)
#define CLKACTIVATIONTIME(x)		(((x) & 3) << 25)
#define ATTACHEDDEVICEPAGELENGTH(x)	(((x) & 3) << 23)
#define WAITREADMONITORING		(1 << 22)
#define WAITWRITEMONITORING		(1 << 21)
#define WAITMONITORINGTIME(x)		(((x) & 3) << 18)
#define WAITPINSELECT(x)		(((x) & 3) << 16)
#define DEVICESIZE(x)			(((x) & 3) << 12)
#define DEVICESIZE_8BIT			DEVICESIZE(0)
#define DEVICESIZE_16BIT		DEVICESIZE(1)
#define DEVICETYPE(x)			(((x) & 3) << 10)
#define DEVICETYPE_NOR			DEVICETYPE(0)
#define DEVICETYPE_NAND			DEVICETYPE(2)
#define MUXADDDATA(n)			((n) << 8)
#define TIMEPARAGRANULARITY		(1 << 4)
#define GPMCFCLKDIVIDER(x)		(((x) & 3) << 0)

/* Values for GPMC_CONFIG2 - CS timing */
#define CSWROFFTIME(x)			(((x) & 0x1f) << 16)
#define CSRDOFFTIME(x)			(((x) & 0x1f) <<  8)
#define CSEXTRADELAY			(1 << 7)
#define CSONTIME(x)			(((x) &	 0xf) <<  0)

/* Values for GPMC_CONFIG3 - nADV timing */
#define ADVWROFFTIME(x)			(((x) & 0x1f) << 16)
#define ADVRDOFFTIME(x)			(((x) & 0x1f) <<  8)
#define ADVEXTRADELAY			(1 << 7)
#define ADVONTIME(x)			(((x) &	 0xf) <<  0)

/* Values for GPMC_CONFIG4 - nWE and nOE timing */
#define WEOFFTIME(x)			(((x) & 0x1f) << 24)
#define WEEXTRADELAY			(1 << 23)
#define WEONTIME(x)			(((x) &	 0xf) << 16)
#define OEOFFTIME(x)			(((x) & 0x1f) <<  8)
#define OEEXTRADELAY			(1 << 7)
#define OEONTIME(x)			(((x) &	 0xf) <<  0)

/* Values for GPMC_CONFIG5 - RdAccessTime and CycleTime timing */
#define PAGEBURSTACCESSTIME(x)		(((x) &	 0xf) << 24)
#define RDACCESSTIME(x)			(((x) & 0x1f) << 16)
#define WRCYCLETIME(x)			(((x) & 0x1f) <<  8)
#define RDCYCLETIME(x)			(((x) & 0x1f) <<  0)

/* Values for GPMC_CONFIG6 - misc timings */
#define WRACCESSTIME(x)			(((x) & 0x1f) << 24)
#define WRDATAONADMUXBUS(x)		(((x) &	0xf) << 16)
#define CYCLE2CYCLEDELAY(x)		(((x) &	0xf) << 8)
#define CYCLE2CYCLESAMECSEN		(1 << 7)
#define CYCLE2CYCLEDIFFCSEN		(1 << 6)
#define BUSTURNAROUND(x)		(((x) &	0xf) << 0)

/* Values for GPMC_CONFIG7 - CS address mapping configuration */
#define MASKADDRESS(x)			(((x) &	0xf) << 8)
#define CSVALID				(1 << 6)
#define BASEADDRESS(x)			(((x) & 0x3f) << 0)

#define GPMC_CLK		100
#define NS_TO_CK(ns)		DIV_ROUND_UP((ns) * GPMC_CLK, 1000)

#define _CSWOFF			NS_TO_CK(tCS_NAND + tCH_NAND)
#define _CSROFF			NS_TO_CK(tCEA_NAND + tRP_NAND - tREA_NAND)

#define _ADWOFF			NS_TO_CK(0)	/* ? */
#define _ADROFF			NS_TO_CK(0)	/* ? */
#define _ADON			NS_TO_CK(0)	/* ? */

#define _WEOFF			NS_TO_CK(tCS_NAND)
#define _WEON			NS_TO_CK(tCS_NAND - tWP_NAND)

#define _OEOFF			NS_TO_CK(tCS_NAND + tCH_NAND)
#define _OEON			NS_TO_CK(tCEA_NAND - tREA_NAND)

#define _RDACC			NS_TO_CK(tCEA_NAND)
#define _WRCYC			NS_TO_CK(tWC_NAND)
#define _RDCYC			NS_TO_CK(tRC_NAND)

#define _WRACC			NS_TO_CK(tALS_NAND)
#define _WBURST			NS_TO_CK(0)
#define _CSHIGH			NS_TO_CK(tADL_NAND)
#define _BTURN			NS_TO_CK(tRHW_NAND)

#define TX48_NAND_GPMC_CONFIG1	(GPMCFCLKDIVIDER(0) |		\
				(0 << 4) /* TIMEPARAGRANULARITY */ | \
				MUXADDDATA(0) |			\
				DEVICETYPE_NAND |		\
				DEVICESIZE_8BIT |		\
				WAITPINSELECT(0) |		\
				WAITMONITORINGTIME(0) |		\
				(0 << 21) /* WAITWRITEMONITORING */ | \
				(0 << 22) /* WAITREADMONITORING */ | \
				ATTACHEDDEVICEPAGELENGTH(0) |	\
				CLKACTIVATIONTIME(0) |		\
				(0 << 27) /* WRITETYPE */ |	\
				(0 << 28) /* WRITEMULTIPLE */ |	\
				(0 << 29) /* READTYPE */ |	\
				(0 << 30) /* READMULTIPLE */ |	\
				(0 << 31) /* WRAPBURST */)
/* CONFIG2: Chip Select */
#define TX48_NAND_GPMC_CONFIG2	(CSWROFFTIME(_CSWOFF) |		\
				CSRDOFFTIME(_CSROFF) |		\
				CSONTIME(0) |			\
				(0 << 7) /* CSEXTRADELAY */)
/* CONFIG3: ADV/ALE */
#define TX48_NAND_GPMC_CONFIG3	(ADVWROFFTIME(_ADWOFF) |	\
				ADVRDOFFTIME(_ADROFF) |		\
				(0 << 7) /* ADVEXTRADELAY */ |	\
				ADVONTIME(_ADON))
/* CONFIG4: WE/OE */
#define TX48_NAND_GPMC_CONFIG4	(WEOFFTIME(_WEOFF) |		\
				(0 << 23) /* WEEXTRADELAY */ |	\
				WEONTIME(_WEON) |		\
				OEOFFTIME(_OEOFF) |		\
				(0 << 7) /* OEEXTRADELAY */ |	\
				OEONTIME(_OEON))
/* CONFIG5: Cycle Timing */
#define TX48_NAND_GPMC_CONFIG5	(PAGEBURSTACCESSTIME(0) |	\
				RDACCESSTIME(_RDACC) |		\
				WRCYCLETIME(_WRCYC) |		\
				RDCYCLETIME(_RDCYC))
/* CONFIG6: Rest of the Pack */
#define TX48_NAND_GPMC_CONFIG6	(WRACCESSTIME(_WRACC) |		\
				WRDATAONADMUXBUS(_WBURST) |	\
				CYCLE2CYCLEDELAY(_CSHIGH) |	\
				CYCLE2CYCLESAMECSEN |		\
				(0 << 6) /* CYCLE2CYCLEDIFFCSEN */ | \
				BUSTURNAROUND(_BTURN))
