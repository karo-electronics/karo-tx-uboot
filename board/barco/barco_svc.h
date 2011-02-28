/********************************************************************
 *
 * Unless otherwise specified, Copyright (C) 2004-2005 Barco Control Rooms
 *
 * $Source$
 * $Revision$
 * $Author$
 * $Date$
 *
 * Last ChangeLog Entry
 * $Log$
 * Revision 1.1.3.2  2011-02-28 14:52:53  lothar
 * imported Ka-Ro specific additions to U-Boot 2009.08 for TX28
 *
 * Revision 1.2  2005/02/21 12:48:58  mleeman
 * update of copyright years (feedback wd)
 *
 * Revision 1.1  2005/02/14 09:31:07  mleeman
 * renaming of files
 *
 * Revision 1.1  2005/02/14 09:23:46  mleeman
 * - moved 'barcohydra' directory to a more generic barco; since we will be
 *   supporting and adding multiple boards
 *
 * Revision 1.1  2005/02/08 15:40:19  mleeman
 * modified and added platform files
 *
 * Revision 1.2  2005/01/25 08:05:04  mleeman
 * more cleanup of the code
 *
 * Revision 1.1  2004/07/20 08:49:55  mleeman
 * Working version of the default and nfs kernel booting.
 *
 *
 *******************************************************************/

#ifndef _LOCAL_BARCOHYDRA_H_
#define _LOCAL_BARCOHYDRA_H_

#include <flash.h>
#include <asm/io.h>

/* Defines for the barcohydra board */
#ifndef CONFIG_SYS_FLASH_ERASE_SECTOR_LENGTH
#define CONFIG_SYS_FLASH_ERASE_SECTOR_LENGTH (0x10000)
#endif

#ifndef CONFIG_SYS_DEFAULT_KERNEL_ADDRESS
#define CONFIG_SYS_DEFAULT_KERNEL_ADDRESS (CONFIG_SYS_FLASH_BASE + 0x30000)
#endif

#ifndef CONFIG_SYS_WORKING_KERNEL_ADDRESS
#define CONFIG_SYS_WORKING_KERNEL_ADDRESS (0xFFE00000)
#endif


typedef struct SBootInfo {
	unsigned int address;
	unsigned int size;
	unsigned char state;
}TSBootInfo;

/* barcohydra.c */
int checkboard(void);
phys_size_t initdram(int board_type);
void pci_init_board(void);
void check_flash(void);
int write_flash(char *addr, char value);
TSBootInfo* find_boot_info(void);
void final_boot(void);
#endif
