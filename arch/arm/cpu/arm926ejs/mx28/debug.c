/*
 * Boot Prep common file
 *
 * Copyright 2008-2009 Freescale Semiconductor
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <stdarg.h>
//#include <stdlib.h>

#include <common.h>

void printhex(int data)
{
	int i;
	char c;

	for (i = sizeof(int) * 2 - 1; i >= 0; i--) {
		c = data >> (i * 4);
		c &= 0xf;
		if (c > 9)
			serial_putc(c - 10 + 'A');
		else
			serial_putc(c + '0');
	}
}

void printdec(int data)
{
	unsigned int m;
	char str[10] = { 0, };
	int i;

	if (data == 0) {
		serial_putc('0');
		return;
	} else if (data < 0) {
		serial_putc('-');
		data = -data;
	}
	m = data;
	for (i = 0; m > 0; i++) {
		str[i] = m % 10 + '0';
		m /= 10;
	}
	for (i--; i >= 0; i--) {
		serial_putc(str[i]);
	}
}

void dprintf(const char *fmt, ...)
{
	va_list args;
	const char *fp = fmt;

	if (!fmt)
		return;

	va_start(args, fmt);
	while (*fp) {
		if (*fp == '%') {
			fp++;
			switch (*fp) {
			case 'c':
				serial_putc(va_arg(args, int));
				break;

			case 's':
				serial_puts(va_arg(args, const char *));
				break;

			case 'd':
			case 'u':
			case 'i':
				printdec(va_arg(args, int));
				break;

			case 'p':
				serial_puts("0x");
			case 'x':
			case 'X':
				printhex(va_arg(args, int));
				break;

			case '%':
				serial_putc('%');
				break;

			default:
				dprintf("\nUnsupported format string token '%c'(%x) in '%s'\n",
					*fp, *fp, fmt);
			}
		} else {
			if (*fp == '\n')
				serial_putc('\r');
			serial_putc(*fp);
		}
		fp++;
	}
	va_end(args);
}
