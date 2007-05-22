
#include <config.h>
#include <version.h>
#include <asm/arch/pxa-regs.h>


#define		DAVICOM_RDF		4
#define		DAVICOM_RDN 	2
#define		DAVICOM_RT		4	/* VLIO */

#define		DEFAULT_RDF		15
#define		DEFAULT_RDN		15
#define		DEFAULT_RT		0




DRAM_SIZE:  .long   CFG_DRAM_SIZE

/* wait for coprocessor write complete */
.macro CPWAIT reg
	mrc	p15,0,\reg,c2,c0,0
	mov	\reg,\reg
	sub	pc,pc,#4
.endm


.macro wait time
	ldr		r2, =OSCR
	mov		r3, #0
	str		r3, [r2]
0:
	ldr		r3, [r2]
	cmp		r3, \time
	bls		0b
.endm


	// macro to print a string
	// modifies r9-r12
.macro PRINT_STRING_FF address
	ldr		r9, =\address			
	ldr		r10, =FFLSR
	ldr		r11, =FFTHR
	
	
12:
	ldr		r12, [r10]		// LSR
	and		r12, r12, #32
	cmp		r12, #32
	bne		12b
	
	ldrb	r12, [r9], #1
	str		r12, [r11]
	cmp		r12, #0
	bne		12b
.endm















/*
 *	Memory setup is already done by system boot loader
 */

.globl lowlevel_init
lowlevel_init:

	mov	r8, lr		@ save link register contents

#if 0
	b 930f
__string_good_1:

	.ascii "\r\nup to now, it is ok \r\n\0"     
	.balign 4

930:
	PRINT_STRING_FF __string_good_1

#endif 







	
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@																																@
@																																@
@			first enable the clocks we need for the board																		@
@																																@
@																																@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	

	ldr		r0, =CKENA
	ldr		r2, [r0]
	ldr		r3, =(CKENA_4_NAND | CKENA_10_SRAM | CKENA_9_SMC | CKENA_8_DMC | CKENA_22_FFUART)
	orr		r2, r2, r3
	str		r2, [r0]




	ldr		r0, =CKENB
	ldr		r2, [r0]
	ldr 	r3, =CKENB_7_GPIO
	orr		r2, r2, r3
	str		r2, [r0]
	


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@																																@
@																																@
@			init GPIO pins now																									@
@																																@
@																																@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

                                                                                                        


__init_GPIO_pins_start:

	
	
	
	@	must set the GPIOs up before any chip selects will work
	@	first, clear all values
	ldr		r1,	=0xffffffff
	@ reset all output registers
	ldr		r0,	=GPCR0
	str		r1, [r0], #4
	str		r1, [r0], #4
	str		r1, [r0]
	ldr		r0,	=GPCR3
	str		r1, [r0]



    add     r1, pc,   #(__hal_gpio_init_table_0 - (.+8))         	@ load table base 
	mov		r3, #GPIO_HIGH						@ feature mask of output = high/low
	
	
	ldr		r2, =GPSR0							
	bl		hal_init_gpio_register				@ Output Set Register for GPIO [31..0]
	bl		hal_init_gpio_register				@ Output Set Register for GPIO [63..32]
	bl		hal_init_gpio_register				@ Output Set Register for GPIO [95..64]
	ldr		r2, =GPSR3							
	bl		hal_init_gpio_register				@ Output Set Register for GPIO [127..96]









    add     r1, pc,   #(__hal_gpio_init_table_0 - (.+8))         	@ load table base 
	mov		r3, #GPIO_DIR_OUTPUT				@ feature mask of output direction
	
	
	ldr		r2, =GPDR0							
	bl		hal_init_gpio_register				@ Output Direction Register for GPIO [31..0]
	bl		hal_init_gpio_register				@ Output Direction Register for GPIO [63..32]
	bl		hal_init_gpio_register				@ Output Direction Register for GPIO [95..64]
	ldr		r2, =GPDR3							
	bl		hal_init_gpio_register				@ Output Direction Register for GPIO [127..96]







    add     r1, pc,   #(__hal_gpio_init_table_0 - (.+8))         	@ load table base 
	mov		r3, #GPIO_REDGE						@ feature mask of rising edge
	
	
	ldr		r2, =GRER0							
	bl		hal_init_gpio_register				@ Output Rising Edge Register for GPIO [31..0]
	bl		hal_init_gpio_register				@ Output Rising Edge Register for GPIO [63..32]
	bl		hal_init_gpio_register				@ Output Rising Edge Register for GPIO [95..64]
	ldr		r2, =GRER3							
	bl		hal_init_gpio_register				@ Output Rising Edge Register for GPIO [127..96]




    add     r1, pc,   #(__hal_gpio_init_table_0 - (.+8))         	@ load table base 
	mov		r3, #GPIO_FEDGE						@ feature mask of rising edge
	
	
	ldr		r2, =GFER0							
	bl		hal_init_gpio_register				@ Output Falling Edge Register for GPIO [31..0]
	bl		hal_init_gpio_register				@ Output Falling Edge Register for GPIO [63..32]
	bl		hal_init_gpio_register				@ Output Falling Edge Register for GPIO [95..64]
	ldr		r2, =GFER3							         
	bl		hal_init_gpio_register				@ Output Falling Edge Register for GPIO [127..96]





	@ reset all edge detect registers
	ldr		r1,	=0xffffffff
	@ reset all output registers
	ldr		r0,	=GEDR0
	str		r1, [r0], #4
	str		r1, [r0], #4
	str		r1, [r0]
	ldr		r0,	=GEDR3
	str		r1, [r0]
		
__init_GPIO_pins_end:	












@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@																																@
@																																@
@			   init MPFR now																									@
@																																@
@																																@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

                                                                                                        


__init_MPFR_start:
	

	ldr		r2,		=0x40e10000										@ physical base address of MFPRs
    add     r1,     pc,  #(__hal_MFPR_init_table - (.+8))         	@ Address of data for MFPR settings 


1:
	ldrh	r3, [r1], #2					@ load offset of MFPR
	cmp		r3, #0
	beq		__init_MFPR_end					@ if offset = 0 -> end of table
	add		r3, r3, r2						@ r3 = MFPR address now

	ldrh	r4, [r1], #2					@ load bits [31..16]
	mov		r4, r4, lsl #16					@ shift it up 16 bits
	ldrh	r5, [r1], #2					@ load bits [15..0]
	add		r4,	r4,	r5

	str		r4, [r3]						@ store data
	ldr		r4, [r3]						@ dummy read to be sure that the value is stored
	b		1b



		
	
		
__init_MFPR_end:	


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@																																@
@																																@
@			   Initialize static Memory Controller																				@
@																																@
@																																@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

                                                                                                        


__init_Static_Mem_Contr_start:


	ldr r0, =CSMSADRCFG		@ Clock Configuration Register
	mov r1, #2	
	str r1, [r0]



	@ initialise the CS Address Configuration Register
	
	ldr r0, =CSADRCFG0		@ only synchronus flash is allowed
	ldr r1, =CSADRCFG_ALW1	@ not used in the moment
	str r1, [r0]

	ldr r0, =CSADRCFG1		@ only synchronus flash is allowed
	ldr r1, =CSADRCFG_ALW1	@ not used in the moment
	str r1, [r0]

	ldr r0, =CSADRCFG2		@ only synchronus flash is allowed
	ldr r1, =(CSADRCFG_INFTYPE_VLIO | CSADRCFG_INFTYPE_BASE0 | CSADRCFG_AdrCfg_FullLatch | CSADRCFG_Split16 | CSADRCFG_ALW7 | CSADRCFG_ALT_1S1H)	@ used for Davicom Ethernet
	str r1, [r0]

	ldr r0, =CSADRCFG3		@ only synchronus flash is allowed
	ldr r1, =CSADRCFG_ALW1	@ not used in the moment
	str r1, [r0]






	ldr r0, =MSC0		
	ldr r1, = (DEFAULT_RDN<<24) | (DEFAULT_RDF<<20) | (1<<19) | (DEFAULT_RT<<16) | (DEFAULT_RDN<<8) | (DEFAULT_RDF<<4) | (1<<3) | DEFAULT_RT
	str r1, [r0]
	ldr r0, =MSC1
	ldr r1, =(DEFAULT_RDN<<24) | (DEFAULT_RDF<<20) | (1<<19) | (DEFAULT_RT<<16) | (DAVICOM_RDN<<8) | (DAVICOM_RDF<<4) | (1<<3) | DAVICOM_RT
	str r1, [r0]


	@@@ Compact Flash Setup @@@
	ldr r0, =MCMEM0
	ldr r1, =0x0003c80f					@ 4081
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
	mov r1, #2		@Set the CIT (Card Is There) bit
	str r1, [r0]




__init_Static_Mem_Contr_end:





@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@																																@
@																																@
@			   clear the RDH bit in the ASCR register																			@
@																																@
@																																@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@



	ldr		r0, =ASCR
	ldr		r1,	[r0]
	ldr		r2, =0x80000000
	bic		r1, r1, r2
	str		r1, [r0]








@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@																																@
@																																@
@			   undo Reset for DAVICOM           																				@
@																																@
@																																@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@



	ldr		r0, =GPCR3
	ldr		r1,	=0x00000080
	str		r1, [r0]





	/* ---------------------------------------------------------------- */
	/* End lowlevel_init							 */
	/* ---------------------------------------------------------------- */

endlowlevel_init:

	mov	pc, r8



hal_init_gpio_register:
	@ r1 = table address
	@ r2 = register address
	@ r3 = feature mask

	mov r4, #1						@ mask register
	mov r5, #0						@ this is the register where the value is composed

1:
	ldrb	r6,	[r1], #1			@ load table entry, increase pointer
	tst		r6, r3					@ test the table entry with feature mask
	orrne 	r5, r5, r4				@ OR mask bit to value if Z=0, means bit was 1

	movs 	r4, r4, lsl #1			@ shift mask bit
	bne 	1b

	str		r5, [r2], #4			@ store register and increase address
	mov		pc, lr					@ jump back to calling function






                                       					

	.balign 32



__hal_MFPR_init_table:                                                          
        .short     	0x0124,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO0	-> GPIO Input (GPIO0 key)
        .short     	0x0128,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO1 	-> GPIO Input (unused) 
        .short     	0x012C, 	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO2 	-> RDY  
        .short     	0x0130,		0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO3 	-> nCS2 (Ethernet) 
        .short     	0x0134,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO4 	-> GPIO Input (unused)  
        .short     	0x028C,		0x0000,		AF_SEL_3 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO5 	-> nPIOR  
        .short     	0x0290, 	0x0000,		AF_SEL_3 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO6 	-> nPIOW 
        .short     	0x0294,   	0x0000,		AF_SEL_3 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO7 	-> nIOS16
        .short     	0x0298,		0x0000,		AF_SEL_3 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO8 	-> nPWAIT 
        .short     	0x029C,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO9 	-> GPIO Input (USB Power Detect)  
        .short     	0x0458,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO10 	-> GPIO Input (USB Cable Detect)  
        .short     	0x02A0,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO11 	-> GPIO Input (unused)  
        .short     	0x02A4,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO12 	-> GPIO Input (unused)  
        .short		0x02A8,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO13 	-> GPIO Input (unused)  
        .short    	0x02AC,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO14 	-> GPIO Output (Backlight Enable) 
        .short		0x02B0,		0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO15 	-> GPIO Output (nMIC_PWR)   
        .short     	0x02B4, 	0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO16 	-> GPIO Input (CF-Card BVD1)  
        .short     	0x02B8,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO17 	-> GPIO Input (CF-Card BVD2)
        .short     	0x02BC,     0x0000,		AF_SEL_4 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO18 	-> MM1_DAT0
        .short     	0x02C0,     0x0000,		AF_SEL_4 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO19 	-> MM1_DAT1
        .short     	0x02C4,     0x0000,		AF_SEL_4 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO20 	-> MM1_DAT2
        .short     	0x02C8,     0x0000,		AF_SEL_4 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO21 	-> MM1_DAT3
        .short     	0x02CC,     0x0000,		AF_SEL_4 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO22 	-> MM1_MMCLK
        .short		0x02D0,     0x0000,		AF_SEL_4 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO23 	-> MM1_MMCMD
        .short     	0x02D4,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO24 	-> GPIO Input (unused)
        .short     	0x02D8,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO25 	-> GPIO Input (unused)
        .short     	0x02DC,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO26 	-> GPIO Input (unused)
        .short     	0x0400,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO27 	-> GPIO Input (unused)
        .short     	0x0404,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO28 	-> GPIO Input (unused)
        .short     	0x0408,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO29 	-> GPIO Input (unused)
        .short     	0x040C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO30 	-> GPIO Input (ETN Wakeup)
        .short		0x0410,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO31 	-> GPIO Input (ETN Interrupt)
        .short     	0x0414, 	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO32 	-> SCL
        .short     	0x0418,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO33 	-> SDA
        .short     	0x041C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO34 	-> GPIO Input (AC97 IRQ)
        .short     	0x0420,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO35 	-> AC97_SDATAIN0
        .short     	0x0424,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO36 	-> GPIO Input (unused)
        .short     	0x0428,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO37 	-> AC97_SDATAOUT
        .short     	0x042C,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO38 	-> AC97_SYNC
        .short     	0x0430,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO39 	-> AC97_BITCLK
        .short     	0x0434,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO40 	-> nAC97_Reset
        .short     	0x0438,     0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO41 	-> FF_RxD
        .short     	0x043C,     0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO42 	-> FF_TxD
        .short     	0x0440,     0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO43 	-> FF_CTS		
        .short     	0x0444,     0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO44 	-> FF_DCD
        .short     	0x0448,     0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO45 	-> FF_DSR
        .short     	0x044C,     0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO46 	-> FF_RI
        .short     	0x0450,     0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO47 	-> FF_DTR
        .short     	0x0454, 	0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO48 	-> FF_RTS  
        .short     	0x045C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO49 	-> GPIO Input (unused)           
        .short     	0x0460,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO50 	-> GPIO Input (unused)           
        .short     	0x0464,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO51 	-> GPIO Input (unused)           
        .short     	0x0468,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO52 	-> GPIO Input (unused)           
        .short     	0x046C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO53 	-> GPIO Input (unused)           
        .short     	0x0470,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO54 	-> GPIO Input (unused)           
        .short     	0x0474,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO55 	-> GPIO Input (unused)           
        .short     	0x0478,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO56 	-> GPIO Input (unused)
        .short     	0x047C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO57 	-> GPIO Input (unused)           
        .short     	0x0480,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO58 	-> GPIO Input (unused)           
        .short     	0x0484,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO59 	-> GPIO Input (unused)
        .short     	0x0488,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO60 	-> GPIO Input (unused)
        .short     	0x048C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO61 	-> GPIO Input (unused)
        .short     	0x0490,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO62 	-> GPIO Input (unused)
        .short     	0x04B4,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO63 	-> LDD[8] 
        .short     	0x04B8,		0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO64 	-> LDD[9]      
        .short     	0x04BC,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO65 	-> LDD[10]     
        .short     	0x04C0,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO66 	-> LDD[11]     
        .short     	0x04C4,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO67 	-> LDD[12]     
        .short     	0x04C8,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO68 	-> LDD[13]     
        .short     	0x04CC,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO69 	-> LDD[14]     
        .short     	0x04D0,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO70 	-> LDD[15]     
        .short     	0x04D4,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO71 	-> LDD[16]     
        .short     	0x04D8,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO72 	-> LDD[17]     
        .short     	0x04DC,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO73 	-> GPIO Input (LCD Header)   
        .short     	0x04F0,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO74 	-> GPIO Input (LCD Header) 
        .short     	0x04F4,     0x0000,		AF_SEL_0 | 	DRIVE_SLOW_10mA | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO75 	-> GPIO Output (userLED red) 
        .short     	0x04F8,     0x0000,		AF_SEL_0 | 	DRIVE_SLOW_10mA | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO76 	-> GPIO Output (userLED green) 
        .short     	0x04FC,     0x0000,		AF_SEL_0 | 	DRIVE_SLOW_10mA | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO77 	-> GPIO Output (userLED red) 
        .short     	0x0500,     0x0000,		AF_SEL_0 | 	DRIVE_SLOW_10mA | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO78 	-> GPIO Output (userLED green) 
        .short     	0x0504,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO79 	-> GPIO Input (unused) 
        .short     	0x0508, 	0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO80 	-> GPIO Input (unused)   
        .short     	0x050C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO81 	-> GPIO Input (unused)  
        .short     	0X0510,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO82 	-> GPIO Input (unused)  
        .short     	0X0514,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO83 	-> GPIO Input (unused)  
        .short     	0x0518,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO84 	-> GPIO Input (unused)  
        .short     	0x051C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO85 	-> GPIO Input (unused)  
        .short     	0x0520,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO86 	-> GPIO Input (unused)  
        .short     	0x0524,   	0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO87 	-> GPIO Input (unused)  
        .short     	0x0528,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO88 	-> GPIO Input (unused)  
        .short     	0x052C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO89 	-> GPIO Input (unused)  
        .short     	0x0530,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO90 	-> GPIO Input (unused) 
        .short     	0x0534,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO91 	-> GPIO Input (unused) 
        .short     	0x0538,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO92 	-> GPIO Input (unused) 
        .short     	0x053C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO93 	-> GPIO Input (unused) 
        .short     	0x0540,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO94 	-> GPIO Input (unused) 
        .short     	0x0544,   	0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO95 	-> GPIO Input (unused) 
        .short     	0x0548, 	0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO96 	-> GPIO Input (unused)   
        .short     	0x054C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO97 	->  CF_IREQ_RDY
        .short     	0x0550,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO98 	->  CF_CD      
        .short     	0x0600,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO99 	->  CF_RESET   
        .short     	0x0604,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |            	| PULL_SEL			@ MFPR GPIO100 	->  GPIO Input (Boot option) 
        .short     	0x0608,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |            	| PULL_SEL			@ MFPR GPIO101 	->  GPIO Input (Boot option) 
        .short     	0x060C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  				| PULL_SEL			@ MFPR GPIO102 	->  GPIO Input (Boot option) 
        .short     	0x0610,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO103 	->  GPIO Output (ETN_RST)
        .short     	0x0614,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO104 	->  GPIO Output (nSD_PWR)
        .short     	0x0618,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO105 	->  GPIO Input (nSD_CD) 
        .short     	0x061C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO106 	->  GPIO Input (nSD_WP) 
        .short     	0x0620,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO107 	-> ICP_TxD 
        .short     	0x0624,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO108 	-> ICP_RxD 
        .short     	0x0628,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO109 	-> BT_RTS  
        .short     	0x062C,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO110 	-> BT_RxD  
        .short     	0x0630,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO111 	-> BT_TxD  
        .short     	0x0634, 	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN		| PULL_SEL			@ MFPR GPIO112 	-> BT_CTS    
        .short     	0x0638,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO113 	-> GPIO Input (unused)    
        .short     	0x063C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO114 	-> GPIO Input (unused)   
        .short     	0x0640,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO115 	-> GPIO Input (unused)    
        .short     	0x0644,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO116 	-> GPIO Input (unused)    
        .short     	0x0648,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO117 	-> GPIO Input (unused)    
        .short     	0x064C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO118 	-> GPIO Input (unused)    
        .short     	0x0650,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO119 	-> GPIO Input (unused)    
        .short     	0x0654,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO120 	-> GPIO Input (unused)    
        .short     	0x0658,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO121 	-> GPIO Input (unused)    
        .short     	0x065C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO122 	-> GPIO Input (unused)   
        .short     	0x0660,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO123 	-> GPIO Input (unused)   
        .short     	0x0664,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO124 	-> GPIO Input (unused)   
        .short     	0x0668,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO125 	-> GPIO Input (unused)   
        .short     	0x066C,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO126 	-> GPIO Input (unused)   
        .short     	0x0670,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO127 	-> GPIO Input (User Key)  
                                       					                                                                        	  
        .short     	0x0674,  	0x0000,		AF_SEL_2 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	| PULL_SEL			@ MFPR GPIO0_2 	 ->  1Wire   
        .short     	0x0678,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO1_2 	 ->  unused  
        .short     	0x067C,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	| PULL_SEL			@ MFPR GPIO2_2 	 ->  GPIO Output (USBHPEN1 USB Host Power Enable)
        .short     	0x0680,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	| PULL_SEL			@ MFPR GPIO3_2 	 ->  GPIO Input (USBHPWR1 USB Host Power Fault)
        .short     	0x0684,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO4_2 	 ->  GPIO Input (unused)  
        .short     	0x0688,     0x0000,		AF_SEL_0 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO5_2 	 ->  GPIO Input (unused)  
        .short     	0x0494,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO6_2 	 ->  LDD[0]  
        .short     	0x0498,   	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO7_2 	 ->  LDD[1]  
        .short     	0x049C,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO9_2 	 ->  LDD[2]  
        .short     	0x04A0,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO9_2 	 ->  LDD[3]  
        .short     	0x04A4,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO10_2  ->  LDD[4]  
        .short     	0x04A8,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO11_2  ->  LDD[5]  
        .short     	0x04AC,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO12_2  ->  LDD[6]  
        .short     	0x04B0,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO13_2  ->  LDD[7]  
        .short     	0x04E0,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO14_2  ->  FCLK    
        .short     	0x04E4,   	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO15_2  ->  LCLK    
        .short     	0x04E8,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO16_2  ->  PCLK    
        .short		0x04EC,     0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ MFPR GPIO17_2  ->  BIAS    






@@        .short		0x23C,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_ADDR0
@@        .short		0x240,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_ADDR1
@@        .short		0x244,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_ADDR2
@@        .short		0x248,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_ADDR3                                
@@        .short		0x238,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	| PULL_SEL			@ nLLA
@@        .short	    0x204,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_CLE_NOE
@@		.short		0x208,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_ALE_WE1
@@		.short		0x20C,  	0x0000,		AF_SEL_1 | 	 		    EDGE_CLEAR                          			@ Reserved (INTEL BBU writes this value)
@@		.short		0x210,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_SCLK_E
@@		.short		0x214,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	    | PULL_SEL			@ nBE0
@@		.short		0x218,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	    | PULL_SEL			@ nBE1
@@		.short		0x21C,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_ALE_WE2               
@@        .short		0x220,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLDOWN_EN	| PULL_SEL			@ DF_INT_RnB
@@        .short		0x224,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	    | PULL_SEL			@ DF_nCS0
@@        .short		0x228,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	    | PULL_SEL			@ DF_nCS1
@@        .short		0x22C,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	    | PULL_SEL			@ DF_nWE
@@        .short		0x230,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	    | PULL_SEL			@ DF_nRE
@@        .short		0x234,  	0x0000,		AF_SEL_1 | 	DRIVE_FAST_4mA  | EDGE_CLEAR |  PULLUP_EN	    | PULL_SEL			@ nLUA
		.short		0x0000,		0x0000,		0x0000																	@ table end																								                                       					
                                       					
	



	.equ GPIO_DIR_INPUT, 	0
	.equ GPIO_DIR_OUTPUT, 	1
	.equ GPIO_LOW,			0
	.equ GPIO_HIGH,			2
	.equ GPIO_REDGE, 		0x80
	.equ GPIO_FEDGE, 		0x40
	.equ GPIO_NOEDGE, 		0
	




	.balign 4
__hal_gpio_init_table_0:                       					
/*GPIO  0 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO0		-> GPIO Input (user key)                                 
/*GPIO  1 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO1 	-> unused                                   
/*GPIO  2 */		.byte	GPIO_DIR_OUTPUT |	GPIO_NOEDGE | GPIO_LOW	    @ GPIO2 	-> GPIO Output (USBHPEN1 USB Host Power Enable)                             
/*GPIO  3 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO3 	-> GPIO Input (USBHPWR1 USB Host Power Fault)                               
/*GPIO  4 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE	| GPIO_LOW	    @ GPIO4 	-> unused                                   
/*GPIO  5 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO5 	-> unused                                               
/*GPIO  6 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO6 	-> unused                                               
/*GPIO  7 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO7 	-> unused                                               
/*GPIO  8 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO8 	-> unused                                               
/*GPIO  9 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO9 	-> GPIO Input (USB Power Detect)                         
/*GPIO 10 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO10 	-> GPIO Input (USB Cable Detect)                         
/*GPIO 11 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO11 	-> input                                  
/*GPIO 12 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO12 	-> input                                  
/*GPIO 13 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO13 	-> input                                  
/*GPIO 14 */		.byte  	GPIO_DIR_OUTPUT	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO14 	-> GPIO Output (Backlight Enable)                        
/*GPIO 15 */		.byte  	GPIO_DIR_OUTPUT |	GPIO_NOEDGE | GPIO_HIGH	    @ GPIO15 	-> GPIO Output (nMIC_PWR)                                
/*GPIO 16 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE	| GPIO_LOW	    @ GPIO16 	-> GPIO Input (CF-Card BVD1)                             
/*GPIO 17 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO17 	-> GPIO Input (CF-Card BVD2)                             
/*GPIO 18 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO18 	-> unused                                  
/*GPIO 19 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO19 	-> unused                                  
/*GPIO 20 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO20 	-> unused                                  
/*GPIO 21 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO21 	-> unused                                  
/*GPIO 22 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO22 	-> unused                                  
/*GPIO 23 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO23 	-> unused                                  
/*GPIO 24 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO24 	-> unused                                  
/*GPIO 25 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO25 	-> unused                                  
/*GPIO 26 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO26 	-> unused                                  
/*GPIO 27 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO27 	-> unused                                  
/*GPIO 28 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO28 	-> unused                                  
/*GPIO 29 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO29 	-> unused                                  
/*GPIO 30 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO30 	-> GPIO Input (ETN Wakeup)                               
/*GPIO 31 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO31 	-> GPIO Input (ETN Interrupt)                            

__hal_gpio_init_table_32:                                            
/*GPIO 32 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO32 	-> unused                                
/*GPIO 33 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO33 	-> unused                                
/*GPIO 34 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO34 	-> unused                                
/*GPIO 35 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO35 	-> unused                                
/*GPIO 36 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE	| GPIO_LOW	    @ GPIO36 	-> unused                                
/*GPIO 37 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO37 	-> unused                                
/*GPIO 38 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO38 	-> unused                                
/*GPIO 39 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO39 	-> unused                                
/*GPIO 40 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO40 	-> unused                                
/*GPIO 41 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO41 	-> unused                                  
/*GPIO 42 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO42 	-> unused                                  
/*GPIO 43 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO43 	-> unused                                  
/*GPIO 44 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO44 	-> unused                                  
/*GPIO 45 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO45 	-> unused                                  
/*GPIO 46 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO46 	-> unused                                  
/*GPIO 47 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO47 	-> unused                                  
/*GPIO 48 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE	| GPIO_LOW 	    @ GPIO48 	-> unused                                  
/*GPIO 49 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO49 	-> unused                                  
/*GPIO 50 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO50 	-> unused                                  
/*GPIO 51 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO51 	-> unused                                  
/*GPIO 52 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO52 	-> unused                                  
/*GPIO 53 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO53 	-> unused                                  
/*GPIO 54 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO54 	-> unused                                  
/*GPIO 55 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO55 	-> unused                                  
/*GPIO 56 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO56 	-> unused                                  
/*GPIO 57 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO57 	-> unused                                  
/*GPIO 58 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO58 	-> unused                                  
/*GPIO 59 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO59 	-> unused                                  
/*GPIO 60 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO60 	-> unused                                  
/*GPIO 61 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO61 	-> unused                                  
/*GPIO 62 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO62 	-> unused                                  
/*GPIO 63 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO63 	-> unused                                  

__hal_gpio_init_table_64:                                            
/*GPIO 64 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO64 	-> unused                                               
/*GPIO 65 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO65 	-> unused                                               
/*GPIO 66 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO66 	-> unused                                               
/*GPIO 67 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO67 	-> unused                                               
/*GPIO 68 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE	| GPIO_LOW	    @ GPIO68 	-> unused                                               
/*GPIO 69 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO69 	-> unused                                               
/*GPIO 70 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO70 	-> unused                                               
/*GPIO 71 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO71 	-> unused                                               
/*GPIO 72 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO72 	-> unused                                               
/*GPIO 73 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO73 	-> GPIO Input (LCD Header)                               
/*GPIO 74 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO74 	-> GPIO Input (LCD Header)                               
/*GPIO 75 */		.byte  	GPIO_DIR_OUTPUT	|	GPIO_NOEDGE | GPIO_HIGH	    @ GPIO75 	-> GPIO Output (userLED red)                             
/*GPIO 76 */		.byte  	GPIO_DIR_OUTPUT |	GPIO_NOEDGE | GPIO_HIGH	    @ GPIO76 	-> GPIO Output (userLED green)                           
/*GPIO 77 */		.byte  	GPIO_DIR_OUTPUT |	GPIO_NOEDGE | GPIO_HIGH	    @ GPIO77 	-> GPIO Output (userLED red)                             
/*GPIO 78 */		.byte  	GPIO_DIR_OUTPUT |	GPIO_NOEDGE | GPIO_HIGH 	@ GPIO78 	-> GPIO Output (userLED green)                           
/*GPIO 79 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO79 	-> unused                                   
/*GPIO 80 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE	| GPIO_LOW 	    @ GPIO80 	-> unused                                   
/*GPIO 81 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO81 	-> unused                                   
/*GPIO 82 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO82 	-> unused                                   
/*GPIO 83 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO83 	-> unused                                   
/*GPIO 84 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO84 	-> unused                                   
/*GPIO 85 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO85 	-> unused                                   
/*GPIO 86 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO86 	-> unused                                   
/*GPIO 87 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO87 	-> unused                                   
/*GPIO 88 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO88 	-> unused                                   
/*GPIO 89 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW	    @ GPIO89 	-> unused                                   
/*GPIO 90 */		.byte  	GPIO_DIR_OUTPUT	|	GPIO_NOEDGE | GPIO_HIGH     @ GPIO90 	-> unused  			Test Test Test Test Test Test Test Test Test                                 
/*GPIO 91 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO91 	-> unused                                   
/*GPIO 92 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO92 	-> unused                                   
/*GPIO 93 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO93 	-> unused                                   
/*GPIO 94 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO94 	-> unused                                   
/*GPIO 95 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO95 	-> unused                                   

__hal_gpio_init_table_96:                                            
/*GPIO 96 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO96 	-> unused                                      
/*GPIO 97 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO97 	-> GPIO Input (CF_IREQ_RDY)                                             
/*GPIO 98 */		.byte	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO98 	-> GPIO Input (CF_CD)                                                   
/*GPIO 99 */		.byte	GPIO_DIR_OUTPUT |	GPIO_NOEDGE | GPIO_HIGH 	@ GPIO99 	-> GPIO Output (CF_RESET)                                                
/*GPIO 100 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE	| GPIO_LOW 	    @ GPIO100 	-> GPIO Input (Boot option)                                
/*GPIO 101 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO101 	-> GPIO Input (Boot option)                                
/*GPIO 102 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO102 	-> GPIO Input (Boot option)                                
/*GPIO 103 */		.byte  	GPIO_DIR_OUTPUT |	GPIO_NOEDGE | GPIO_HIGH	    @ GPIO103 	-> GPIO Output (ETN_RST)                                   
/*GPIO 104 */		.byte  	GPIO_DIR_OUTPUT |	GPIO_NOEDGE | GPIO_HIGH	    @ GPIO104 	-> GPIO Output (nSD_PWR)                                   
/*GPIO 105 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO105 	-> GPIO Input (nSD_CD)                                     
/*GPIO 106 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO106 	-> GPIO Input (nSD_WP)                                     
/*GPIO 107 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO107 	-> unused                                                  
/*GPIO 108 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO108 	-> unused                                                  
/*GPIO 109 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO109 	-> unused                                                  
/*GPIO 110 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO110 	-> unused                                                  
/*GPIO 111 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO111 	-> unused                                                  
/*GPIO 112 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE	| GPIO_LOW 	    @ GPIO112 	-> unused                                                  
/*GPIO 113 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO113 	-> unused                                      
/*GPIO 114 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO114 	-> unused                                      
/*GPIO 115 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO115 	-> unused                                      
/*GPIO 116 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO116 	-> unused                                      
/*GPIO 117 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO117 	-> unused                                      
/*GPIO 118 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO118 	-> unused                                      
/*GPIO 119 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO119 	-> unused                                      
/*GPIO 120 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW 	    @ GPIO120 	-> unused                                      
/*GPIO 121 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO121 	-> unused                                   
/*GPIO 122 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO122 	-> unused                                      
/*GPIO 123 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO123 	-> unused                                      
/*GPIO 124 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO124 	-> unused                                      
/*GPIO 125 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO125 	-> unused                                      
/*GPIO 126 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO126 	-> unused                                      
/*GPIO 127 */		.byte  	GPIO_DIR_INPUT 	|	GPIO_NOEDGE | GPIO_LOW      @ GPIO127 	-> GPIO Input (User Key)                                    
                                                                                            	                                                            







