/*
 * file: testpreempt.c
 */
#include <8051.h>
#include "preemptive.h"

__data __at (0x3A) char isEmpty;
__data __at (0x3B) char buffer;
__data __at (0x3C) char nextChar;

void Producer(void)
{
    nextChar = 'A';
    while (1)
    {
        if (isEmpty){
            __critical { //SDCC suggests that you can surround the code fragment using the __critical
                //construct, to ensure that the two shared vars are accessed atomically
                buffer = nextChar;
                isEmpty = !isEmpty;
                nextChar = (nextChar == 'Z') ? 'A' : nextChar + 1;  // Cycle through 'A'-'Z'
            }
        }
    }
}

void Consumer(void)
{
    SCON = 0x50;  // Serial mode 1 (8-bit UART) and enable receiver
    TMOD |= 0x20; // Timer 1 in mode 2 (8-bit auto-reload); 
    TH1 = (char)-6;   // Load value for 4800 baud rate (assuming 11.0592 MHz clock)
    TR1 = 1;      // Start Timer 1
    TI = 1;
    while (1)
    {
        if (!isEmpty){
            while (!TI);  // Wait for transmission to complete
            __critical { //SDCC suggests that you can surround the code fragment using the __critical
            //construct, to ensure that the two shared vars are accessed atomically
                SBUF = buffer;  // Send data to serial port
                TI = 0;  // Clear transmission flag
                isEmpty = !isEmpty;
            }
        }
    }
}
void main(void)
{
    isEmpty = 1;  // Initialize buffer as empty
    buffer = ' ';
    ThreadCreate(Producer);
    Consumer();
}

void _sdcc_gsinit_startup(void)
{
    __asm
        LJMP _Bootstrap
    __endasm;
}

void _mcs51_genRAMCLEAR(void) {}
void _mcs51_genXINIT(void) {}
void _mcs51_genXRAMCLEAR(void) {}
void timer0_ISR(void) __interrupt(1) {
    __asm
        ljmp _myTimer0Handler
    __endasm;
}

