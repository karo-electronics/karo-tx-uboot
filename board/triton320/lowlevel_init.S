
#include <config.h>
#include <version.h>
#include <asm/arch/pxa-regs.h>

#define		DAVICOM_RDF	8
#define		DAVICOM_RDN	4
#define		DAVICOM_RT	4	/* VLIO */

#define		DEFAULT_RDF	15
#define		DEFAULT_RDN	15
#define		DEFAULT_RT	0

	.equ	UART_LSR,	FFLSR - FFUART
	.equ	UART_THR,	FFTHR - FFUART

DRAM_SIZE:  .long   CFG_DRAM_SIZE

/* R12 (IP) must not be modified by this code! */

	// macro to print a string
	// modifies r9-r11
.macro PRINT_STRING_FF address
#if 0
	ldr	r9, =\address
	ldr	r10, =FFUART
2:
	ldr	r11, [r10, #UART_LSR]
	ands	r11, r11, #32
	bne	2b

	ldrb	r11, [r9], #1
	cmp	r11, #0
	strne	r11, [r10, #UART_THR]
	bne	2b
#endif
.endm

.macro	set_mask reg, mask, val
	ldr	r0, =(\reg)
	ldr	r2, =~(\mask)
	ldr	r1, [r0]
	ldr	r3, =(\val)
	and	r1, r1, r2
	orr	r1, r1, r3
	str	r1, [r0]
.endm

.macro	wait_mask reg, mask, val
	ldr	r0, =(\reg)
	ldr	r2, =(\mask)
	ldr	r3, =(\val)
1:
	ldr	r1, [r0]
	and	r1, r1, r2
	cmp	r1, r3
	bne	1b
.endm

/*
 *	Memory setup is already done by system boot loader
 */
.globl lowlevel_init
lowlevel_init:
	mov	r8, lr		@ save link register contents
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@												@
@ first enable the clocks we need for the board							@
@												@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
__init_clocks:
#if 1
	/* Fixup VCC_APPS voltage settings */
	ldr	r0, =AVCR
	ldr	r1, [r0]
	mvn	r2, #0x20
	and	r1, r1, r2
	orr	r1, #0x0f
	str	r1, [r0]
#endif
	ldr	r0, =CKENA
	ldr	r3, =(CKENA_RSRVD | CKENA_4_NAND | CKENA_10_SRAM | CKENA_9_SMC | CKENA_8_DMC | CKENA_22_FFUART)
	ldr	r2, [r0]
	orr	r2, r2, r3
	str	r2, [r0]

	ldr	r0, =CKENB
	ldr	r3, =(CKENB_RSRVD | CKENB_7_GPIO)
	ldr	r2, [r0]
	orr	r2, r2, r3
	str	r2, [r0]

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@											@
@	init GPIO pins now								@
@											@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
__init_GPIO_pins_start:
	@	must set the GPIOs up before any chip selects will work
	@	first, clear all values
	ldr	r1, =0xffffffff
	@ reset all output registers
	ldr	r0, =GPCR0
	str	r1, [r0], #4
	str	r1, [r0], #4
	str	r1, [r0]
	ldr	r0, =GPCR3
	str	r1, [r0]

	ldr	r1, =__hal_gpio_init_table_0
	mov	r3, #GPIO_HIGH			@ feature mask of output = high/low

	ldr	r2, =GPSR0
	bl	hal_init_gpio_register		@ Output Set Register for GPIO [31..0]
	bl	hal_init_gpio_register		@ Output Set Register for GPIO [63..32]
	bl	hal_init_gpio_register		@ Output Set Register for GPIO [95..64]
	ldr	r2, =GPSR3
	bl	hal_init_gpio_register		@ Output Set Register for GPIO [127..96]

	ldr	r1, =__hal_gpio_init_table_0
	mov	r3, #GPIO_DIR_OUTPUT		@ feature mask of output direction

	ldr	r2, =GPDR0
	bl	hal_init_gpio_register		@ Output Direction Register for GPIO [31..0]
	bl	hal_init_gpio_register		@ Output Direction Register for GPIO [63..32]
	bl	hal_init_gpio_register		@ Output Direction Register for GPIO [95..64]
	ldr	r2, =GPDR3
	bl	hal_init_gpio_register		@ Output Direction Register for GPIO [127..96]

	ldr	r1, =__hal_gpio_init_table_0
	mov	r3, #GPIO_REDGE			@ feature mask of rising edge

	ldr	r2, =GRER0
	bl	hal_init_gpio_register		@ Output Rising Edge Register for GPIO [31..0]
	bl	hal_init_gpio_register		@ Output Rising Edge Register for GPIO [63..32]
	bl	hal_init_gpio_register		@ Output Rising Edge Register for GPIO [95..64]
	ldr	r2, =GRER3
	bl	hal_init_gpio_register		@ Output Rising Edge Register for GPIO [127..96]

	ldr	r1, =__hal_gpio_init_table_0
	mov	r3, #GPIO_FEDGE			@ feature mask of rising edge

	ldr	r2, =GFER0
	bl	hal_init_gpio_register		@ Output Falling Edge Register for GPIO [31..0]
	bl	hal_init_gpio_register		@ Output Falling Edge Register for GPIO [63..32]
	bl	hal_init_gpio_register		@ Output Falling Edge Register for GPIO [95..64]
	ldr	r2, =GFER3
	bl	hal_init_gpio_register		@ Output Falling Edge Register for GPIO [127..96]

	@ reset all edge detect registers
	ldr	r1, =0xffffffff
	@ reset all output registers
	ldr	r0, =GEDR0
	str	r1, [r0], #4
	str	r1, [r0], #4
	str	r1, [r0]
	ldr	r0, =GEDR3
	str	r1, [r0]
__init_GPIO_pins_end:

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@											@
@	init MPFR now									@
@											@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
__init_MPFR_start:
	ldr	r2, =0x40e10000			@ physical base address of MFPRs
	add     r1, pc, #(__hal_MFPR_init_table - (.+8))	@ Address of data for MFPR settings
1:
	ldrh	r3, [r1], #2			@ load offset of MFPR
	cmp	r3, #0
	beq	__init_MFPR_end			@ if offset = 0 -> end of table
	add	r3, r3, r2			@ r3 = MFPR address now

	ldrh	r4, [r1], #2			@ load bits [15..0]

	str	r4, [r3]			@ store data
	b	1b
__init_MFPR_end:
	@ Spec. Update MP-5454 (Rev. 0.75 page 93)
	@ clear EMPI SCLK
	ldr	r1, =0x48100100
	@ r3 is zero as exit condition from the loop above
	str	r3, [r1, #0x2c]
	ldr	r3, [r1]
	orr	r3, #0x40000000
	str	r3, [r1]

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@											@
@	Initialize static Memory Controller						@
@											@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
__init_Static_Mem_Contr_start:
	ldr r0, =CSMSADRCFG		@ Clock Configuration Register
	mov r1, #2
	str r1, [r0]

	@ initialise the CS Address Configuration Register

	ldr r0, =CSADRCFG0		@ only synchronus flash is allowed
	ldr r1, =CSADRCFG_ALW1		@ not used at the moment
	str r1, [r0]

	ldr r0, =CSADRCFG1		@ only synchronus flash is allowed
	ldr r1, =CSADRCFG_ALW1		@ not used at the moment
	str r1, [r0]

	ldr r0, =CSADRCFG2		@ only synchronus flash is allowed
	ldr r1, =(CSADRCFG_INFTYPE_VLIO | CSADRCFG_INFTYPE_BASE0 | CSADRCFG_AdrCfg_FullLatch | CSADRCFG_Split16 | CSADRCFG_ALW7 | CSADRCFG_ALT_1S1H)	@ used for Davicom Ethernet
	str r1, [r0]

	ldr r0, =CSADRCFG3		@ only synchronus flash is allowed
	ldr r1, =CSADRCFG_ALW1		@ not used at the moment
	str r1, [r0]

#if 0
	ldr r0, =MSC0
	ldr r1, = (DEFAULT_RDN<<24) | (DEFAULT_RDF<<20) | (1<<19) | (DEFAULT_RT<<16) | (DEFAULT_RDN<<8) | (DEFAULT_RDF<<4) | (1<<3) | DEFAULT_RT
	str r1, [r0]
#endif
	ldr r0, =MSC1
	ldr r1, =(DEFAULT_RDN<<24) | (DEFAULT_RDF<<20) | (1<<19) | (DEFAULT_RT<<16) | (DAVICOM_RDN<<8) | (DAVICOM_RDF<<4) | (1<<3) | DAVICOM_RT
	str r1, [r0]

	@@@ Compact Flash Setup @@@
	ldr r0, =MCMEM0
	ldr r1, =0x0003c80f			@ 4081
	str r1, [r0]

	ldr r0, =MCATT0
	ldr r1, =0x0003c80f
	str r1, [r0]

	ldr r0, =MCIO0
	ldr r1, =0x0003c80f
	str r1, [r0]

	ldr r0, =CSADRCFG_P
	ldr r1, =0x003e080C
	str r1, [r0]

	ldr r0, =MECR
	mov r1, #2				@ Set the CIT (Card Is There) bit
	str r1, [r0]

__init_Static_Mem_Contr_end:
	@ assert UP2OCR[HSOE] (enable USB device port2)
	ldr	r0, =0x40600020	@ base address of UP2OCR
	ldr	r1, [r0]
	orr	r1, #(1 << 17)	@ UP2OCR_HSOE
	str	r1, [r0]

	@ clear the RDH bit in the ASCR register
	ldr	r0, =ASCR
	ldr	r1, [r0]
	bic	r1, r1, #(1 << 31)		@ RDH
	bic	r1, r1, #0x7			@ mask off the DxS status bits
	str	r1, [r0]

	@ deassert Reset for DAVICOM
	ldr	r0, =GPCR3
	ldr	r1, =0x00000080
	str	r1, [r0]
endlowlevel_init:
	mov	pc, r8


hal_init_gpio_register:
	@ r1 = table address
	@ r2 = register address
	@ r3 = feature mask

	mov	r4, #1		@ mask register
	mov	r5, #0		@ this is the register where the value is composed
1:
	ldrb	r6, [r1], #1	@ load table entry, increase pointer
	tst	r6, r3		@ test the table entry with feature mask
	orrne	r5, r5, r4	@ OR mask bit to value if Z=0, means bit was 1

	movs	r4, r4, lsl #1	@ shift mask bit
	bne	1b

	str	r5, [r2], #4	@ store register and increase address
	mov	pc, lr		@ jump back to calling function

	.balign 32
__hal_MFPR_init_table:
	.short 0x00CC, 1	@ MFPR EMPI SCLK (see Spec. Update Rev 0.75 page 93)
	.short 0x0124, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO0	-> GPIO Input (GPIO0 key)
        .short 0x0128, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO1	-> GPIO Input (unused)
        .short 0x012C, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO2	-> RDY
        .short 0x0130, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO3	-> nCS2 (Ethernet)
        .short 0x0134, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO4	-> GPIO Input (unused)
        .short 0x028C, AF_SEL_3 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO5	-> nPIOR
        .short 0x0290, AF_SEL_3 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO6	-> nPIOW
        .short 0x0294, AF_SEL_3 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO7	-> nIOS16
        .short 0x0298, AF_SEL_3 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO8	-> nPWAIT
        .short 0x029C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO9	-> GPIO Input (USB Power Detect)
        .short 0x0458, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO10	-> GPIO Input (USB Cable Detect)
        .short 0x02A0, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO11	-> GPIO Input (unused)
        .short 0x02A4, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO12	-> GPIO Input (unused)
        .short 0x02A8, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO13	-> GPIO Input (unused)
        .short 0x02AC, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO14	-> GPIO Output (Backlight Enable)
        .short 0x02B0, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO15	-> GPIO Output (nMIC_PWR)
        .short 0x02B4, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO16	-> GPIO Input (CF-Card BVD1)
        .short 0x02B8, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO17	-> GPIO Input (CF-Card BVD2)
        .short 0x02BC, AF_SEL_4 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO18	-> MM1_DAT0
        .short 0x02C0, AF_SEL_4 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO19	-> MM1_DAT1
        .short 0x02C4, AF_SEL_4 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO20	-> MM1_DAT2
        .short 0x02C8, AF_SEL_4 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO21	-> MM1_DAT3
        .short 0x02CC, AF_SEL_4 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO22	-> MM1_MMCLK
        .short 0x02D0, AF_SEL_4 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO23	-> MM1_MMCMD
        .short 0x02D4, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO24	-> GPIO Input (unused)
        .short 0x02D8, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO25	-> GPIO Input (unused)
        .short 0x02DC, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO26	-> GPIO Input (unused)
        .short 0x0400, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO27	-> GPIO Input (unused)
        .short 0x0404, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO28	-> GPIO Input (unused)
        .short 0x0408, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO29	-> GPIO Input (unused)
        .short 0x040C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO30	-> GPIO Input (ETN Wakeup)
        .short 0x0410, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO31	-> GPIO Input (ETN Interrupt)
        .short 0x0414, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO32	-> SCL
        .short 0x0418, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO33	-> SDA
        .short 0x041C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO34	-> GPIO Input (AC97 IRQ)
        .short 0x0420, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO35	-> AC97_SDATAIN0
        .short 0x0424, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO36	-> GPIO Input (unused)
        .short 0x0428, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO37	-> AC97_SDATAOUT
        .short 0x042C, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO38	-> AC97_SYNC
        .short 0x0430, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO39	-> AC97_BITCLK
        .short 0x0434, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO40	-> nAC97_Reset
        .short 0x0438, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO41	-> FF_RxD
        .short 0x043C, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO42	-> FF_TxD
        .short 0x0440, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO43	-> FF_CTS
        .short 0x0444, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO44	-> FF_DCD
        .short 0x0448, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO45	-> FF_DSR
        .short 0x044C, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO46	-> FF_RI
        .short 0x0450, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO47	-> FF_DTR
        .short 0x0454, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO48	-> FF_RTS
        .short 0x045C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO49	-> GPIO Input (unused)
        .short 0x0460, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO50	-> GPIO Input (unused)
        .short 0x0464, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO51	-> GPIO Input (unused)
        .short 0x0468, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO52	-> GPIO Input (unused)
        .short 0x046C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO53	-> GPIO Input (unused)
        .short 0x0470, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO54	-> GPIO Input (unused)
        .short 0x0474, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO55	-> GPIO Input (unused)
        .short 0x0478, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO56	-> GPIO Input (unused)
        .short 0x047C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO57	-> GPIO Input (unused)
        .short 0x0480, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO58	-> GPIO Input (unused)
        .short 0x0484, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO59	-> GPIO Input (unused)
        .short 0x0488, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO60	-> GPIO Input (unused)
        .short 0x048C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO61	-> GPIO Input (unused)
        .short 0x0490, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO62	-> GPIO Input (unused)
        .short 0x04B4, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO63	-> LDD[8]
        .short 0x04B8, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO64	-> LDD[9]
        .short 0x04BC, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO65	-> LDD[10]
        .short 0x04C0, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO66	-> LDD[11]
        .short 0x04C4, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO67	-> LDD[12]
        .short 0x04C8, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO68	-> LDD[13]
        .short 0x04CC, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO69	-> LDD[14]
        .short 0x04D0, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO70	-> LDD[15]
        .short 0x04D4, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO71	-> LDD[16]
        .short 0x04D8, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO72	-> LDD[17]
        .short 0x04DC, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO73	-> GPIO Input (LCD Header)
        .short 0x04F0, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO74	-> GPIO Input (LCD Header)
        .short 0x04F4, AF_SEL_0 | DRIVE_SLOW_10mA | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO75	-> GPIO Output (userLED red)
        .short 0x04F8, AF_SEL_0 | DRIVE_SLOW_10mA | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO76	-> GPIO Output (userLED green)
        .short 0x04FC, AF_SEL_0 | DRIVE_SLOW_10mA | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO77	-> GPIO Output (userLED red)
        .short 0x0500, AF_SEL_0 | DRIVE_SLOW_10mA | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO78	-> GPIO Output (userLED green)
        .short 0x0504, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO79	-> GPIO Input (unused)
        .short 0x0508, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO80	-> GPIO Input (unused)
        .short 0x050C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO81	-> GPIO Input (unused)
        .short 0X0510, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO82	-> GPIO Input (unused)
        .short 0X0514, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO83	-> GPIO Input (unused)
        .short 0x0518, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO84	-> GPIO Input (unused)
        .short 0x051C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO85	-> GPIO Input (unused)
        .short 0x0520, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO86	-> GPIO Input (unused)
        .short 0x0524, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO87	-> GPIO Input (unused)
        .short 0x0528, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO88	-> GPIO Input (unused)
        .short 0x052C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO89	-> GPIO Input (unused)
        .short 0x0530, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO90	-> GPIO Input (unused)
        .short 0x0534, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO91	-> GPIO Input (unused)
        .short 0x0538, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO92	-> GPIO Input (unused)
        .short 0x053C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO93	-> GPIO Input (unused)
        .short 0x0540, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO94	-> GPIO Input (unused)
        .short 0x0544, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO95	-> GPIO Input (unused)
        .short 0x0548, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO96	-> GPIO Input (unused)
        .short 0x054C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO97	->  CF_IREQ_RDY
        .short 0x0550, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO98	->  CF_CD
        .short 0x0600, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO99	->  CF_RESET
        .short 0x0604, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR |               PULL_SEL	@ MFPR GPIO100	->  GPIO Input (Boot option)
        .short 0x0608, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR |               PULL_SEL	@ MFPR GPIO101	->  GPIO Input (Boot option)
        .short 0x060C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR |	       PULL_SEL	@ MFPR GPIO102	->  GPIO Input (Boot option)
        .short 0x0610, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO103	->  GPIO Output (ETN_RST)
        .short 0x0614, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO104	->  GPIO Output (nSD_PWR)
        .short 0x0618, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO105	->  GPIO Input (nSD_CD)
        .short 0x061C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO106	->  GPIO Input (nSD_WP)
        .short 0x0620, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO107	-> ICP_TxD
        .short 0x0624, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO108	-> ICP_RxD
        .short 0x0628, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO109	-> BT_RTS
        .short 0x062C, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO110	-> BT_RxD
        .short 0x0630, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO111	-> BT_TxD
        .short 0x0634, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO112	-> BT_CTS
        .short 0x0638, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO113	-> GPIO Input (unused)
        .short 0x063C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO114	-> GPIO Input (unused)
        .short 0x0640, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO115	-> GPIO Input (unused)
        .short 0x0644, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO116	-> GPIO Input (unused)
        .short 0x0648, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO117	-> GPIO Input (unused)
        .short 0x064C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO118	-> GPIO Input (unused)
        .short 0x0650, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO119	-> GPIO Input (unused)
        .short 0x0654, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO120	-> GPIO Input (unused)
        .short 0x0658, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO121	-> GPIO Input (unused)
        .short 0x065C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO122	-> GPIO Input (unused)
        .short 0x0660, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO123	-> GPIO Input (unused)
        .short 0x0664, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO124	-> GPIO Input (unused)
        .short 0x0668, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO125	-> GPIO Input (unused)
        .short 0x066C, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO126	-> GPIO Input (unused)
        .short 0x0670, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO127	-> GPIO Input (User Key)

        .short 0x0674, AF_SEL_2 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO0_2	 ->  1Wire
        .short 0x0678, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO1_2	 ->  unused
        .short 0x067C, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO2_2	 ->  GPIO Output (USBHPEN1 USB Host Power Enable)
        .short 0x0680, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ MFPR GPIO3_2	 ->  GPIO Input (USBHPWR1 USB Host Power Fault)
        .short 0x0684, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO4_2	 ->  GPIO Input (unused)
        .short 0x0688, AF_SEL_0 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO5_2	 ->  GPIO Input (unused)
        .short 0x0494, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO6_2	 ->  LDD[0]
        .short 0x0498, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO7_2	 ->  LDD[1]
        .short 0x049C, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO9_2	 ->  LDD[2]
        .short 0x04A0, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO9_2	 ->  LDD[3]
        .short 0x04A4, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO10_2  ->  LDD[4]
        .short 0x04A8, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO11_2  ->  LDD[5]
        .short 0x04AC, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO12_2  ->  LDD[6]
        .short 0x04B0, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO13_2  ->  LDD[7]
        .short 0x04E0, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO14_2  ->  FCLK
        .short 0x04E4, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO15_2  ->  LCLK
        .short 0x04E8, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO16_2  ->  PCLK
        .short 0x04EC, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ MFPR GPIO17_2  ->  BIAS

@@	.short 0x023C, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_ADDR0
@@	.short 0x0240, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_ADDR1
@@	.short 0x0244, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_ADDR2
@@	.short 0x0248, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_ADDR3
@@	.short 0x0238, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ nLLA
@@	.short 0x0204, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_CLE_NOE
@@	.short 0x0208, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_ALE_WE1
@@	.short 0x020C, AF_SEL_1 | 		    EDGE_CLEAR				@ Reserved (INTEL BBU writes this value)
@@	.short 0x0210, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_SCLK_E
@@	.short 0x0214, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ nBE0
@@	.short 0x0218, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ nBE1
@@	.short 0x021C, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_ALE_WE2
@@	.short 0x0220, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLDOWN_EN | PULL_SEL	@ DF_INT_RnB
@@	.short 0x0224, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ DF_nCS0
@@	.short 0x0228, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ DF_nCS1
@@	.short 0x022C, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ DF_nWE
@@	.short 0x0230, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ DF_nRE
@@	.short 0x0234, AF_SEL_1 | DRIVE_FAST_4mA  | EDGE_CLEAR | PULLUP_EN   | PULL_SEL	@ nLUA
	.short 0x0000, 0x0000																	@ table end

	.equ GPIO_DIR_INPUT,	0x00
	.equ GPIO_DIR_OUTPUT,	0x01
	.equ GPIO_LOW,		0x00
	.equ GPIO_HIGH,		0x02
	.equ GPIO_REDGE,	0x80
	.equ GPIO_FEDGE,	0x40
	.equ GPIO_NOEDGE,	0x00

	.balign 4
__hal_gpio_init_table_0:
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO0   -> GPIO Input (user key)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO1   -> unused
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_LOW  @ GPIO2   -> GPIO Output (USBHPEN1 USB Host Power Enable)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO3   -> GPIO Input (USBHPWR1 USB Host Power Fault)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO4   -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO5   -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO6   -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO7   -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO8   -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO9   -> GPIO Input (USB Power Detect)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO10  -> GPIO Input (USB Cable Detect)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO11  -> input
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO12  -> input
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO13  -> input
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_LOW  @ GPIO14  -> GPIO Output (Backlight Enable)
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO15  -> GPIO Output (nMIC_PWR)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO16  -> GPIO Input (CF-Card BVD1)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO17  -> GPIO Input (CF-Card BVD2)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO18  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO19  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO20  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO21  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO22  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO23  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO24  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO25  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO26  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO27  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO28  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO29  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO30  -> GPIO Input (ETN Wakeup)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO31  -> GPIO Input (ETN Interrupt)

__hal_gpio_init_table_32:
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO32  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO33  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO34  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO35  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO36  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO37  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO38  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO39  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO40  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO41  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO42  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO43  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO44  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO45  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO46  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO47  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO48  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO49  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO50  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO51  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO52  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO53  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO54  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO55  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO56  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO57  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO58  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO59  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO60  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO61  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO62  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO63  -> unused

__hal_gpio_init_table_64:
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO64  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO65  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO66  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO67  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO68  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO69  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO70  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO71  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO72  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO73  -> GPIO Input (LCD Header)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO74  -> GPIO Input (LCD Header)
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO75  -> GPIO Output (userLED red)
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO76  -> GPIO Output (userLED green)
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO77  -> GPIO Output (userLED red)
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO78  -> GPIO Output (userLED green)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO79  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO80  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO81  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO82  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO83  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO84  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO85  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO86  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO87  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO88  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO89  -> unused
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO90  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO91  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO92  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO93  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO94  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO95  -> unused

__hal_gpio_init_table_96:
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO96  -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO97  -> GPIO Input (CF_IREQ_RDY)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO98  -> GPIO Input (CF_CD)
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO99  -> GPIO Output (CF_RESET)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO100 -> GPIO Input (Boot option)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO101 -> GPIO Input (Boot option)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO102 -> GPIO Input (Boot option)
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO103 -> GPIO Output (ETN_RST)
	.byte GPIO_DIR_OUTPUT | GPIO_NOEDGE | GPIO_HIGH @ GPIO104 -> GPIO Output (nSD_PWR)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO105 -> GPIO Input (nSD_CD)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO106 -> GPIO Input (nSD_WP)
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO107 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO108 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO109 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO110 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO111 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO112 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO113 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO114 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO115 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO116 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO117 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO118 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO119 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO120 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO121 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO122 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO123 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO124 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO125 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO126 -> unused
	.byte GPIO_DIR_INPUT | GPIO_NOEDGE | GPIO_LOW   @ GPIO127 -> GPIO Input (User Key)
