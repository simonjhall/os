/*
 * print_uart.cpp
 *
 *  Created on: 5 May 2013
 *      Author: simon
 */

#include "print_uart.h"

void PrinterUart::PrintChar(char c)
{
        *sm_pTxAddress = c;
}



