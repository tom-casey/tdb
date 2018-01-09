#include "breakpoint.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ptrace.h>
//insert a breakpoint into the program running at PID(must be being ptraced)
int insert_bp(Breakpoint * bp, pid_t pid) {
    //save the instruction we're overwriting
    bp->save = ptrace(PTRACE_PEEKDATA, pid, bp->addr, NULL);
    printf("Data at %p is %lx\n", bp->addr, bp->save);
    if (bp->save == -1) {
        printf("PTRACE_PEEKDATA got error %d accessing memory at %p\n", errno,
               bp->addr);
    }
    //replace last byte in data with our INT 3(0xcc) instruction
    ptrace(PTRACE_POKEDATA, pid, bp->addr, ((bp->save & ~0xff) | 0xcc));
    printf("New data at %p is %lx\n", bp->addr, ((bp->save & ~0xff) | 0xcc));
    bp->enabled = 1;
    return 1;
}
//replace the interrupt instruction with the original instruction
int remove_bp(Breakpoint * bp, pid_t pid) {
    ptrace(PTRACE_POKEDATA, pid, bp->addr, bp->save);
    bp->enabled = 0;
    return 1;
}
