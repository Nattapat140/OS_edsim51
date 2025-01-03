/*
 * file: test3thread.c
 */
#include <8051.h>
#include "preemptive.h"

// __data __at (0x3A) char isEmpty;
// __data __at (0x3B) char buffer;
// __data __at (0x3C) char nextChar;
__data __at (0x3A) char head;
__data __at (0x3B) char tail;
__data __at (0x3C) char nextChar;
__data __at (0x24) char nextNum;
//to switching between 2 producers
__data __at(0x25) char isConsumer;
__data __at(0x26) char producerNum;
//three semaphores
// Ensures that only one thread accesses the buffer at a time; set = 1 allows one thread at a time
__data __at (0x3D) char mutex;
// Counts the number of full slots in the buffer; if 3 means buffer = empty; 1 slot = 1 element in buffer
__data __at (0x3E) char full;
// Counts the number of empty slots in the buffer; if 0 means buffer = full; 1 slot = 1 empty space in buffer
__data __at (0x3F) char empty;
__data __at (0x20) char BoundedBuffer[3] = {' ', ' ', ' '}; //Bounded Buffer

void Producer1(void)
{
    //Acquires an empty semaphore, then acquires the mutex lock to add an item to the buffer
    nextChar = 'A';
    while (1)
    {
        SemaphoreWait(empty);
        SemaphoreWait(mutex);
        __critical { //SDCC suggests that you can surround the code fragment using the __critical
            //construct, to ensure that the two shared vars are accessed atomically
            BoundedBuffer[tail] = nextChar;
            tail = (tail + 1) % 3;
            nextChar = (nextChar == 'Z') ? 'A' : nextChar + 1;  // Cycle through 'A'-'Z'
        }
        SemaphoreSignal(mutex);
        SemaphoreSignal(full);
    }
}
void Producer2(void)
{
    //Acquires an empty semaphore, then acquires the mutex lock to add an item to the buffer
    nextNum = '0';
    while (1)
    {
        SemaphoreWait(empty);
        SemaphoreWait(mutex);
        __critical { //SDCC suggests that you can surround the code fragment using the __critical
            //construct, to ensure that the two shared vars are accessed atomically
            BoundedBuffer[tail] = nextNum;
            tail = (tail + 1) % 3;
            nextNum = (nextNum == '9') ? '0' : nextNum + 1;  // Cycle through 'A'-'Z'
        }
        SemaphoreSignal(mutex);
        SemaphoreSignal(full);
    }
}

void Consumer(void)
{
    //Acquires a full semaphore, then acquires the mutex lock to remove an item from the buffer
    SCON = 0x50;  // Serial mode 1 (8-bit UART) and enable receiver
    TMOD |= 0x20; // Timer 1 in mode 2 (8-bit auto-reload); 
    TH1 = (char)-6;   // Load value for 4800 baud rate (assuming 11.0592 MHz clock)
    TR1 = 1;      // Start Timer 1
    TI = 1;
    while (1)
    {
        SemaphoreWait(full);
        SemaphoreWait(mutex);
        while (!TI);  // Wait for transmission to complete
        __critical { //SDCC suggests that you can surround the code fragment using the __critical
        //construct, to ensure that the two shared vars are accessed atomically
            SBUF = BoundedBuffer[head];  // Send data to serial port
            TI = 0;  // Clear transmission flag
            head = (head+1) % 3;
        }
        SemaphoreSignal(mutex);
        SemaphoreSignal(empty);
    }
}
void main(void)
{
    SemaphoreCreate(mutex, 1);
    SemaphoreCreate(full, 0);
    SemaphoreCreate(empty, 3);
    head = 0;
    tail = 0;
    BoundedBuffer[0] = ' ';
    BoundedBuffer[1] = ' ';
    BoundedBuffer[2] = ' ';
    isConsumer = 0;
    producerNum = 1;
    ThreadCreate(Producer1);
    ThreadCreate(Producer2);
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