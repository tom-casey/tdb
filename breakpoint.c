#include "breakpoint.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <libexplain/ptrace.h>
int insert_bp(Breakpoint * bp, pid_t pid) {
    bp->save = ptrace(PTRACE_PEEKDATA, pid, bp->addr, NULL);
    printf("Data at %x is %x\n",bp->addr,bp->save);
    if (bp->save == -1) {
        printf("PTRACE_PEEKDATA got error %d accessing memory at %x\n",errno,bp->addr);
        char *explanation = explain_errno_ptrace(errno,PTRACE_PEEKDATA,pid,bp->addr,NULL);
        printf("Explanation: %s\n",explanation);
    }
    //replace last byte in data with our INT 3(0xcc) instruction
    ptrace(PTRACE_POKEDATA, pid, bp->addr, ((bp->save & ~0xff) | 0xcc));
    printf("New data at %x is %x\n",bp->addr,((bp->save & ~0xff) | 0xcc));
    bp->enabled = 1;
    return 1;
}
