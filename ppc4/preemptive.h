/*
 * file: preemptive.h
 *
 * this is the include file for the preemptive multithreading
 * package.  It is to be compiled by SDCC and targets the EdSim51 as
 * the target architecture.
 *
 * CS 3423 Fall 2018
 */

#ifndef __PREEMPTIVE_H__
#define __PREEMPTIVE_H__

#define MAXTHREADS 4 /* not including the scheduler */
/* the scheduler does not take up a thread of its own */
#define CNAME(s) _ ## s
//unique label, keep adding $
#define LABEL_NAME(l) l ## $
#define SemaphoreCreate(s, n) *&s=n

#define SemaphoreWaitBody(s, label) \
{ __asm \
    LABEL_NAME(label):  \
        /*read value of _S into ACC (where S is semaphore) \*/ \
        mov a, CNAME(s) \
        jz LABEL_NAME(label)  \
        /* conditionally jump(s) back to label if ACC <= 0*/ \
        jb ACC.7, LABEL_NAME(label) \
        /*keep decresing S*/ \
        dec  CNAME(s) \
__endasm; \
}
//solved by using C preprocessor's __COUNTER__ name
#define SemaphoreWait(s) SemaphoreWaitBody(s, __COUNTER__)

#define SemaphoreSignal(s) \
{ __asm \
    /* Increase S back when we it completes its operation and comes out of it*/ \
    inc CNAME(s) \
__endasm; \
} 

typedef char ThreadID;
typedef void (*FunctionPtr)(void);

ThreadID ThreadCreate(FunctionPtr);
void ThreadYield(void);
void ThreadExit(void);

#endif // __PREEMPTIVE_H__