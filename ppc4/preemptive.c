#include <8051.h>

#include "preemptive.h"
__data __at (0x30) char savedSP[MAXTHREADS];  // Saved Stack Pointers for threads
// save 4 address for Thread 0 - 3
__data __at (0x34) int threadBitmap;           // Bitmap to track active threads; where each bit represents whether a thread is active. 1010 thread 0 and 3 is working
__data __at (0x2D) int bitmapCheck;             //temp for bitmap checking; check for availability

__data __at (0x2A) ThreadID newThreadID;      // Temporary variable for thread creation
__data __at (0x35) ThreadID currThread;      // ID of the current active thread

__data __at (0x2B) char newThreadStack;       // Stack pointer for the new thread
__data __at (0x2C) char prevSP;               // Holds previous SP during thread creation
__data __at (0x2E) int RegBank;               // set the correct regbank in threadcreate 
//to switching between 2 producers
__data __at(0x25) char isConsumer;
__data __at(0x26) char producerNum;
/*
 * [TODO]
 * define a macro for saving the context of the current thread by
 * 1) push ACC, B register, Data pointer registers (DPL, DPH), PSW
 * 2) save SP into the saved Stack Pointers array
 *   as indexed by the current thread ID.
 * Note that 1) should be written in assembly,
 *     while 2) can be written in either assembly or C
 */
#define SAVESTATE \
   { __asm \
      push ACC          /* Save accumulator register */ \
      push B            /* Save B register */ \
      push DPL          /* Save Data Pointer Low byte */ \
      push DPH          /* Save Data Pointer High byte */ \
      push PSW          /* Save Processor Status Word */ \
   __endasm; \
   } \
   { __asm \
      /* Save the current stack pointer (SP) for the current thread */ \
      mov a, _currThread   /* Load current thread index into A */ \
      add a, #_savedSP     /* Add address of savedSP to A */ \
      mov r0, a            /* Store the address of savedSP[currThread] in r0 */ \
      mov @r0, _SP         /* Store current SP into savedSP[currThread]; savedSP[currThread] = _SP */ \
   __endasm; \
   }

#define RESTORESTATE \
   { __asm \
      /* Load the stack pointer (SP) for the current thread */ \
      mov a, _currThread   /* Load current thread index into A */ \
      add a, #_savedSP     /* Add address of savedSP to A */ \
      mov r1, a            /* Store result in r1 */ \
      mov _SP, @r1         /* Load savedSP[currThread] into SP; _SP = savedSP[currThread]; */ \
   __endasm; \
   } \
   { __asm \
      /* Restore the saved registers */ \
      pop PSW              /* Restore Processor Status Word */ \
      pop DPH              /* Restore Data Pointer High byte */ \
      pop DPL              /* Restore Data Pointer Low byte */ \
      pop B                /* Restore B register */ \
      pop ACC              /* Restore accumulator register */ \
   __endasm; \
   }

/*
 * we declare main() as an extern so we can reference its symbol
 * when creating a thread for it.
 */

extern void main(void);

/*
 * Bootstrap is jumped to by the startup code to make the thread for
 * main, and restore its context so the thread can run.
 */

void Bootstrap(void)
{
   //by default, SP is 0x07.
   TMOD = 0;  // timer 0 mode 0
   IE = 0x82;  // enable timer 0 interrupt; keep consumer polling
               // EA  -  ET2  ES  ET1  EX1  ET0  EX0
   TR0 = 1; // set bit TR0 to start running timer 0
   threadBitmap = 0x00;
   currThread = ThreadCreate(main);  // Create thread for main
   RESTORESTATE;  // Start running main
}

/*
 * ThreadCreate() creates a thread data structure so it is ready
 * to be restored (context switched in).
 * The function pointer itself should take no argument and should
 * return no argument.
 */
ThreadID ThreadCreate(FunctionPtr fp)
{
   EA = 0; // you may set and clear EA bit before and after the function body code.
   if (threadBitmap == (1 << MAXTHREADS) - 1) {
      return -1;
   }

   for (newThreadID = 0; newThreadID < MAXTHREADS; newThreadID++) {
      bitmapCheck = threadBitmap & (1 << newThreadID);
      if (!bitmapCheck) break;  // Found an available slot, find bit 0 in threadBitmap;
   }
   // Mark the thread as active; set bit to 1
   threadBitmap |= (1 << newThreadID);
   // Calculate the starting stack address for the new thread
   // Each thread’s stack is 16 bytes apart.; just want to make sure stack starts at 0x_F
   //b
   newThreadStack = ((newThreadID + 3) << 4) | 0x0F;
   // Save the current stack pointer and set up the new thread's stack
   //c
   prevSP = SP;
   SP = newThreadStack;
   // Push the return address (fp) for the new thread onto the stack
   //d
   __asm
      push DPL
      push DPH 
   __endasm;
   //e
   __asm
      mov A, #0
      push ACC  // ACC
      push ACC  // B
      push ACC  // DPL
      push ACC  // DPH
   __endasm;
   //f
   RegBank = newThreadID << 3; //set the correct back to the correct thread
   __asm
      push _RegBank // PSW 
   __endasm;
   // Save the stack pointer for the newly created thread
   //g
   __asm
      mov a, _newThreadID
      add a, #_savedSP
      mov R0, a
      mov @R0, SP
   __endasm;

   // Restore the original stack pointer 
   // h
   SP = prevSP;
   EA = 1;
   //i
   return newThreadID;
}

/*
 * this is called by a running thread to yield control to another
 * thread.  ThreadYield() saves the context of the current
 * running thread, picks another thread (and set the current thread
 * ID to it), if any, and then restores its state.
 */

void ThreadYield(void)
{
   __critical {
      SAVESTATE;
      do
      {
         // Move to the next thread in a round-robin fashion
         currThread = (currThread + 1) % MAXTHREADS;
         bitmapCheck = threadBitmap & (1 << currThread);
      } while (!bitmapCheck);
      RESTORESTATE;
   }
}

/*
 * ThreadExit() is called by the thread's own code to terminate
 * itself.  It will never return; instead, it switches context
 * to another thread.
 */
void ThreadExit(void)
{
   __critical {
      threadBitmap &= ~(1 << currThread);  // Mark thread as inactive

      do {
         currThread = (currThread + 1) % MAXTHREADS;
      } while (!(threadBitmap & (1 << currThread)));  // Find next valid thread
      RESTORESTATE;
   }
}

//based on ThreadYield
void myTimer0Handler(void)
{  
   EA = 0;
   SAVESTATE;
   do
   {
      // Move to the next thread in a round-robin fashion
      if (isConsumer) {
         currThread = 0; // Consumer
      } 
      else {
         currThread = producerNum;
         producerNum = (producerNum == 1) ? 2 : 1; // Alternate between producer 1 and 2
      }
      isConsumer = !isConsumer;
      bitmapCheck = threadBitmap & (1 << currThread);
   } while (!bitmapCheck);
   RESTORESTATE;
   EA = 1;
   __asm
      reti
   __endasm;
}