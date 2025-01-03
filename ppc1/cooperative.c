#include <8051.h>

#include "cooperative.h"

/*
 * [TODO]
 * declare the static globals here using
 *        __data __at (address) type name; syntax
 * manually allocate the addresses of these variables, for
 * - saved stack pointers (MAXTHREADS)
 * - current thread ID
 * - a bitmap for which thread ID is a valid thread;
 *   maybe also a count, but strictly speaking not necessary
 * - plus any temporaries that you need.
 */
__data __at (0x30) char savedSP[MAXTHREADS];  // Saved Stack Pointers for threads
// save 4 address for Thread 0 - 3
__data __at (0x34) int threadBitmap;           // Bitmap to track active threads; where each bit represents whether a thread is active. 1010 thread 0 and 3 is working
__data __at (0x2D) int bitmapCheck;             //temp for bitmap checking; check for availability

__data __at (0x2A) ThreadID newThreadID;      // Temporary variable for thread creation
__data __at (0x35) ThreadID currThread;      // ID of the current active thread

__data __at (0x2B) char newThreadStack;       // Stack pointer for the new thread
__data __at (0x2C) char prevSP;               // Holds previous SP during thread creation
__data __at (0x2E) int RegBank;               // set the correct regbank in threadcreate
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


/*
 * [TODO]
 * define a macro for restoring the context of the current thread by
 * essentially doing the reverse of SAVESTATE:
 * 1) assign SP to the saved SP from the saved stack pointer array
 * 2) pop the registers PSW, data pointer registers, B reg, and ACC
 * Again, popping must be done in assembly but restoring SP can be
 * done in either C or assembly.
 */
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
    /*
     * [TODO]
     * initialize data structures for threads (e.g., mask)
     *
     * optional: move the stack pointer to some known location
     * only during bootstrapping. by default, SP is 0x07.
     *
     * [TODO]
     *     create a thread for main; be sure current thread is
     *     set to this thread ID, and restore its context,
     *     so that it starts running main().
     */
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
    /*
     * [TODO]
     * check to see we have not reached the max #threads.
     * if so, return -1, which is not a valid thread ID.
     */
    if (threadBitmap == (1 << MAXTHREADS) - 1) return -1;
    /*
     * [TODO]
     *     otherwise, find a thread ID that is not in use,
     *     and grab it. (can check the bit mask for threads),
     *
     * [TODO] below
     * a. update the bit mask
         (and increment thread count, if you use a thread count,
          but it is optional)
       b. calculate the starting stack location for new thread
       c. save the current SP in a temporary
          set SP to the starting location for the new thread
       d. push the return address fp (2-byte parameter to
          ThreadCreate) onto stack so it can be the return
          address to resume the thread. Note that in SDCC
          convention, 2-byte ptr is passed in DPTR.  but
          push instruction can only push it as two separate
          registers, DPL and DPH.
       e. we want to initialize the registers to 0, so we
          assign a register to 0 and push it four times
          for ACC, B, DPL, DPH.  Note: push #0 will not work
          because push takes only direct address as its operand,
          but it does not take an immediate (literal) operand.
       f. finally, we need to push PSW (processor status word)
          register, which consist of bits
           CY AC F0 RS1 RS0 OV UD P
          all bits can be initialized to zero, except <RS1:RS0>
          which selects the register bank.
          Thread 0 uses bank 0, Thread 1 uses bank 1, etc.
          Setting the bits to 00B, 01B, 10B, 11B will select
          the register bank so no need to push/pop registers
          R0-R7.  So, set PSW to
          00000000B for thread 0, 00001000B for thread 1,
          00010000B for thread 2, 00011000B for thread 3.
       g. write the current stack pointer to the saved stack
          pointer array for this newly created thread ID
       h. set SP to the saved SP in step c.
       i. finally, return the newly created thread ID.
     */
    //a
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
        mov A, _newThreadID
        add A, #_savedSP
        mov R0, A
        mov @R0, SP
    __endasm;

    // Restore the original stack pointer 
    // h
    SP = prevSP;
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
    SAVESTATE;
    do
    {
        /*
         * [TODO]
         * do round-robin policy for now.
         * find the next thread that can run and
         * set the current thread ID to it,
         * so that it can be restored (by the last line of
         * this function).
         * there should be at least one thread, so this loop
         * will always terminate.
         */
        // Move to the next thread in a round-robin fashion
        currThread = (currThread + 1) % MAXTHREADS;
        bitmapCheck = threadBitmap & (1 << currThread);
    } while (!bitmapCheck);
    RESTORESTATE;
}

/*
 * ThreadExit() is called by the thread's own code to terminate
 * itself.  It will never return; instead, it switches context
 * to another thread.
 */
void ThreadExit(void)
{
    threadBitmap &= ~(1 << currThread);  // Mark thread as inactive
    /*
     * clear the bit for the current thread from the
     * bit mask, decrement thread count (if any),
     * and set current thread to another valid ID.
     * Q: What happens if there are no more valid threads?
     */
    do {
        currThread = (currThread + 1) % MAXTHREADS;
    } while (!(threadBitmap & (1 << currThread)));  // Find next valid thread
    RESTORESTATE;
}
