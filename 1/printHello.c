/*
 * printHello.c
 *
 *  Created on: 2026年5月19日
 *      Author: Dell
 */

#include "printHello.h"
#include "uart_printf.h"

void printHello()
{
	uart_printf("Hello VeriSilicon!\r\n");
}
