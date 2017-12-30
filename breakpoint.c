#include "breakpoint.h"
#include <unistd.h>
#include <sys/ptrace.h>
int insert_bp(Breakpoint * bp, pid_t pid) {
    bp->save = ptrace(PTRACE_PEEKDATA, pid, bp->addr, NULL);
    if (!(bp->save)) {
        return -1;
    }
    //replace last byte in data with our INT 3(0xcc) instruction
    ptrace(PTRACE_POKEDATA, pid, bp->addr, (bp->save | 0xcc));
    bp->enabled = 1;
    return 1;
}
