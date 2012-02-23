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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __DRIVERS_MTD_NAND_GPMI_NFC_H
#define __DRIVERS_MTD_NAND_GPMI_NFC_H

/*
 *------------------------------------------------------------------------------
 * Fundamental Macros
 *------------------------------------------------------------------------------
 */

#define  NFC_DMA_DESCRIPTOR_COUNT  4

/* Define this macro to enable detailed information messages. */
#define DETAILED_INFO

/* Define this macro to enable event reporting. */
/*#define EVENT_REPORTING*/

/*
 *------------------------------------------------------------------------------
 * Fundamental Data Structures
 *------------------------------------------------------------------------------
 */

#define COMMAND_BUFFER_SIZE 10

/**
 * struct gpmi_nfc_timing - GPMI NFC timing parameters.
 *
 * This structure contains the fundamental timing attributes for the NAND Flash
 * bus and the GPMI NFC hardware.
 *
 * @data_setup_in_ns:          The data setup time, in nanoseconds. Usually the
 *                             maximum of tDS and tWP. A negative value
 *                             indicates this characteristic isn't known.
 * @data_hold_in_ns:           The data hold time, in nanoseconds. Usually the
 *                             maximum of tDH, tWH and tREH. A negative value
 *                             indicates this characteristic isn't known.
 * @address_setup_in_ns:       The address setup time, in nanoseconds. Usually
 *                             the maximum of tCLS, tCS and tALS. A negative
 *                             value indicates this characteristic isn't known.
 * @gpmi_sample_time_in_ns:    A GPMI-specific timing parameter. A negative
 *                             value indicates this characteristic isn't known.
 * @tREA_in_ns:                tREA, in nanoseconds, from the data sheet. A
 *                             negative value indicates this characteristic
 *                             isn't known.
 * @tRLOH_in_ns:               tRLOH, in nanoseconds, from the data sheet. A
 *                             negative value indicates this characteristic
 *                             isn't known.
 * @tRHOH_in_ns:               tRHOH, in nanoseconds, from the data sheet. A
 *                             negative value indicates this characteristic
 *                             isn't known.
 */
struct gpmi_nfc_timing {
	int8_t    data_setup_in_ns;
	int8_t    data_hold_in_ns;
	int8_t    address_setup_in_ns;
	int8_t    gpmi_sample_delay_in_ns;
	int8_t    tREA_in_ns;
	int8_t    tRLOH_in_ns;
	int8_t    tRHOH_in_ns;
};
#if 0
enum nand_device_cell_technology {
	NAND_DEVICE_CELL_TECH_SLC = 0,
	NAND_DEVICE_CELL_TECH_MLC = 1,
};

struct nand_device_info {
	/* End of table marker */
	bool      end_of_table;

	/* Manufacturer and Device codes */
	uint8_t   manufacturer_code;
	uint8_t   device_code;

	/* Technology */
	enum nand_device_cell_technology  cell_technology;

	/* Geometry */
	uint64_t  chip_size_in_bytes;
	uint32_t  block_size_in_pages;
	uint16_t  page_total_size_in_bytes;

	/* ECC */
	uint8_t   ecc_strength_in_bits;
	uint16_t  ecc_size_in_bytes;

	/* Timing */
	int8_t    data_setup_in_ns;
	int8_t    data_hold_in_ns;
	int8_t    address_setup_in_ns;
	int8_t    gpmi_sample_delay_in_ns;
	int8_t    tREA_in_ns;
	int8_t    tRLOH_in_ns;
	int8_t    tRHOH_in_ns;

	/* human readable device description */
	const char  *description;
};

/**
 * struct gpmi_nfc_data - i.MX NFC per-device data.
 *
 * Note that the "device" managed by this driver represents the NAND Flash
 * controller *and* the NAND Flash medium behind it. Thus, the per-device data
 * structure has information about the controller, the chips to which it is
 * connected, and properties of the medium as a whole.
 *
 * @dev:                 A pointer to the owning struct device.
 * @pdev:                A pointer to the owning struct platform_device.
 * @pdata:               A pointer to the device's platform data.
 * @device_info:         A structure that contains detailed information about
 *                       the NAND Flash device.
 * @nfc:                 A pointer to a structure that represents the underlying
 *                       NFC hardware.
 * @rom:                 A pointer to a structure that represents the underlying
 *                       Boot ROM.
 * @mil:                 A collection of information used by the MTD Interface
 *                       Layer.
 */
struct gpmi_nfc_data {
	/* System Interface */
	struct device		       *dev;
	struct platform_device	       *pdev;
	struct gpmi_nfc_platform_data  *pdata;

	/* Resources */
	struct clk    *clock;
	void __iomem *gpmi_regs;
	void __iomem *bch_regs;
	unsigned int bch_interrupt;
	unsigned int dma_low_channel;
	unsigned int dma_high_channel;
	unsigned int dma_interrupt;

//	struct resources	       resources;

	/* NFC HAL */
	/* Hardware attributes. */
	unsigned int	max_chip_count;

	/* Working variables. */
	struct mxs_dma_desc	*dma_descriptors[NFC_DMA_DESCRIPTOR_COUNT];
	int			isr_dma_channel;
	struct completion	dma_done;
	struct completion	bch_done;
	struct gpmi_nfc_timing	timing;
//	struct nfc_hal		*nfc;
	int			current_chip;

	/* MTD Interface Layer */
	struct mtd_info	       mtd;
	struct nand_chip       chip;
	struct nand_ecclayout  oob_layout;
	struct mtd_partition   *partitions;
	unsigned int	       partition_count;

	/* DMA Buffers */
	u8		       *cmd_virt;
	dma_addr_t	       cmd_phys;
	unsigned int	       command_length;

	void		       *page_buffer_virt;
	dma_addr_t	       page_buffer_phys;
	unsigned int	       page_buffer_size;

	void		       *payload_virt;
	dma_addr_t	       payload_phys;

	void		       *auxiliary_virt;
	dma_addr_t	       auxiliary_phys;
};

/**
 * struct nfc_hal - GPMI NFC HAL
 *
 * This structure embodies an abstract interface to the underlying NFC hardware.
 *
 * @version:                       The NFC hardware version.
 * @description:                   A pointer to a human-readable description of
 *                                 the NFC hardware.
 * @max_chip_count:                The maximum number of chips the NFC can
 *                                 possibly support (this value is a constant
 *                                 for each NFC version). This may *not* be the
 *                                 actual number of chips connected.
 * @max_data_setup_cycles:         The maximum number of data setup cycles
 *                                 that can be expressed in the hardware.
 * @max_data_sample_delay_cycles:  The maximum number of data sample delay
 *                                 cycles that can be expressed in the hardware.
 * @max_dll_clock_period_in_ns:    The maximum period of the GPMI clock that the
 *                                 sample delay DLL hardware can possibly work
 *                                 with (the DLL is unusable with longer
 *                                 periods). At HALF this value, the DLL must be
 *                                 configured to use half-periods.
 * @dma_descriptors:               A pool of DMA descriptors.
 * @isr_dma_channel:               The DMA channel with which the NFC HAL is
 *                                 working. We record this here so the ISR knows
 *                                 which DMA channel to acknowledge.
 * @dma_done:                      The completion structure used for DMA
 *                                 interrupts.
 * @bch_done:                      The completion structure used for BCH
 *                                 interrupts.
 * @timing:                        The current timing configuration.
 * @init:                          Initializes the NFC hardware and data
 *                                 structures. This function will be called
 *                                 after everything has been set up for
 *                                 communication with the NFC itself, but before
 *                                 the platform has set up off-chip
 *                                 communication. Thus, this function must not
 *                                 attempt to communicate with the NAND Flash
 *                                 hardware.
 * @set_geometry:                  Configures the NFC hardware and data
 *                                 structures to match the physical NAND Flash
 *                                 geometry.
 * @set_geometry:                  Configures the NFC hardware and data
 *                                 structures to match the physical NAND Flash
 *                                 geometry.
 * @exit:                          Shuts down the NFC hardware and data
 *                                 structures. This function will be called
 *                                 after the platform has shut down off-chip
 *                                 communication but while communication with
 *                                 the NFC itself still works.
 * @clear_bch:                     Clears a BCH interrupt (intended to be called
 *                                 by a more general interrupt handler to do
 *                                 device-specific clearing).
 * @is_ready:                      Returns true if the given chip is ready.
 * @begin:                         Begins an interaction with the NFC. This
 *                                 function must be called before *any* of the
 *                                 following functions so the NFC can prepare
 *                                 itself.
 * @end:                           Ends interaction with the NFC. This function
 *                                 should be called to give the NFC a chance to,
 *                                 among other things, enter a lower-power
 *                                 state.
 * @send_command:                  Sends the given buffer of command bytes.
 * @send_data:                     Sends the given buffer of data bytes.
 * @read_data:                     Reads data bytes into the given buffer.
 * @send_page:                     Sends the given given data and OOB bytes,
 *                                 using the ECC engine.
 * @read_page:                     Reads a page through the ECC engine and
 *                                 delivers the data and OOB bytes to the given
 *                                 buffers.
 */

struct nfc_hal {
	/* Hardware attributes. */
	const unsigned int	version;
	const char		*description;
	const unsigned int	max_chip_count;
	const unsigned int	max_data_setup_cycles;
	const unsigned int	max_data_sample_delay_cycles;
	const unsigned int	max_dll_clock_period_in_ns;

	/* Working variables. */
	struct mxs_dma_desc	*dma_descriptors[NFC_DMA_DESCRIPTOR_COUNT];
	int			isr_dma_channel;
	struct completion	dma_done;
	struct completion	bch_done;
	struct gpmi_nfc_timing	timing;

	/* Configuration functions. */
	int   (*init)	     (struct gpmi_nfc_data *);
	int   (*set_geometry)(struct gpmi_nfc_data *);
	int   (*set_timing)  (struct gpmi_nfc_data *,
						const struct gpmi_nfc_timing *);
	void  (*exit)	     (struct gpmi_nfc_data *);

	/* Call these functions to begin and end I/O. */

	void  (*begin)	     (struct gpmi_nfc_data *);
	void  (*end)	     (struct gpmi_nfc_data *);

	/* Call these I/O functions only between begin() and end(). */

	void  (*clear_bch)   (struct gpmi_nfc_data *);
	int   (*is_ready)    (struct gpmi_nfc_data *, unsigned chip);
	int   (*send_command)(struct gpmi_nfc_data *, unsigned chip,
				dma_addr_t buffer, unsigned length);
	int   (*send_data)   (struct gpmi_nfc_data *, unsigned chip,
				dma_addr_t buffer, unsigned length);
	int   (*read_data)   (struct gpmi_nfc_data *, unsigned chip,
				dma_addr_t buffer, unsigned length);
	int   (*send_page)   (struct gpmi_nfc_data *, unsigned chip,
				dma_addr_t payload, dma_addr_t auxiliary);
	int   (*read_page)   (struct gpmi_nfc_data *, unsigned chip,
				dma_addr_t payload, dma_addr_t auxiliary);
};

/**
 * struct boot_rom_helper - Boot ROM Helper
 *
 * This structure embodies the interface to an object that assists the driver
 * in making decisions that relate to the Boot ROM.
 *
 * @version:                    The Boot ROM version.
 * @description:                A pointer to a human-readable description of the
 *                              Boot ROM.
 * @swap_block_mark:            Indicates that the Boot ROM will swap the block
 *                              mark with the first byte of the OOB.
 * @set_geometry:               Configures the Boot ROM geometry.
 * @check_transcription_stamp:  Checks for a transcription stamp. This pointer
 *                              is ignored if swap_block_mark is set.
 * @write_transcription_stamp:  Writes a transcription stamp. This pointer
 *                              is ignored if swap_block_mark is set.
 */

struct boot_rom_helper {
	const unsigned int  version;
	const char	    *description;
	const int	    swap_block_mark;
	int  (*set_geometry)		 (struct gpmi_nfc_data *);
	int  (*check_transcription_stamp)(struct gpmi_nfc_data *);
	int  (*write_transcription_stamp)(struct gpmi_nfc_data *);
};

/*
 *------------------------------------------------------------------------------
 * External Symbols
 *------------------------------------------------------------------------------
 */

/* Event Reporting */

#if defined(EVENT_REPORTING)
#if 0
	extern void gpmi_nfc_start_event_trace(char *description);
	extern void gpmi_nfc_add_event(char *description, int delta);
	extern void gpmi_nfc_stop_event_trace(char *description);
	extern void gpmi_nfc_dump_event_trace(void);
#endif
#else
	#define gpmi_nfc_start_event_trace(description)	 do {} while (0)
	#define gpmi_nfc_add_event(description, delta)	 do {} while (0)
	#define gpmi_nfc_stop_event_trace(description)	 do {} while (0)
	#define gpmi_nfc_dump_event_trace()		 do {} while (0)
#endif /* EVENT_REPORTING */

#endif /* 0 */

#endif /* __DRIVERS_MTD_NAND_GPMI_NFC_H */
