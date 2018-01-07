#include "breakpoint.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <libexplain/ptrace.h>
int insert_bp(Breakpoint * bp, pid_t pid) {
    bp->save = ptrace(PTRACE_PEEKDATA, pid, bp->addr, NULL);
    printf("Data at %p is %lx\n", bp->addr, bp->save);
    if (bp->save == -1) {
        printf("PTRACE_PEEKDATA got error %d accessing memory at %p\n", errno,
               bp->addr);
        const char *explanation =
            explain_errno_ptrace(errno, PTRACE_PEEKDATA, pid, bp->addr, NULL);
        printf("Explanation: %s\n", explanation);
    }
    //replace last byte in data with our INT 3(0xcc) instruction
    ptrace(PTRACE_POKEDATA, pid, bp->addr, ((bp->save & ~0xff) | 0xcc));
    printf("New data at %p is %lx\n", bp->addr, ((bp->save & ~0xff) | 0xcc));
    bp->enabled = 1;
    return 1;
}
int remove_bp(Breakpoint * bp, pid_t pid) {
    ptrace(PTRACE_POKEDATA, pid, bp->addr, bp->save);
    bp->enabled = 0;
    return 1;
}
