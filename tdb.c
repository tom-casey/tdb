#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <assert.h>
#include "tdb.h"
#include "breakpoint.h"
int fork_to_child(int argc, char *argv[]);
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [OPTION] \"program to debug\"", argv[0]);
    }
    char *brkpt;
    pid_t pid = -1;
    int sflag = 0;
    int c;
    while ((c = getopt(argc, argv, "sb:p:")) != -1) {
        switch (c) {
           case 's':
               sflag = 1;
               break;
           case 'b':
               brkpt = optarg;
               break;
           case 'p':
               pid = (pid_t) atoi(optarg);
               break;
           case '?':
               fprintf(stderr, "Bad option -%c", optopt);
           default:
               exit(EXIT_FAILURE);
        }
    }
    char **bps = NULL;
    if (brkpt) {
        bps = str_split(brkpt, ',');
    }
    if (pid == -1) {
        pid = fork_to_child(argc - optind, &argv[optind]);
    } else {
        ptrace(PTRACE_ATTACH, pid, NULL, 0);
    }
    if (sflag == 1) {
        ptrace(PTRACE_SINGLESTEP, pid, NULL, 0);
    }
    Breakpoint **bplist = malloc(sizeof(Breakpoint *) * 256);
    if (bps) {
        int num_bpts = sizeof(bps) / sizeof(char *);
        if (num_bpts > 256) {
            fprintf(stderr, "Error: Too many breakpoints");
        }
        for (int i = 0; i < num_bpts; i++) {
            bplist[i] = malloc(sizeof(Breakpoint));
            sscanf(bps[i], "%x", &bplist[i]->addr);
            if (!(bplist[i]->addr)) {
                fprintf(stderr,
                        "%s is not a valid memory address. Skipping\n",
                        bps[i]);
            }
            insert_bp(bplist[i]);
        }
    }
    int status;
    while (pid == waitpid(pid, &status, 0)) {
        if (WIFEXITED(status)) {
            printf("Child exited with status %d\n", WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status)) {
            printf("Child got killing signal %d\n", WTERMSIG(status));
        }
        if (WIFSTOPPED(status)) {
          redo:
            printf
                ("Child caught signal %d\n(q)uit\t(r)egisters\t(s)inglestep\t(c)ontinue\n",
                 WSTOPSIG(status));
            char input = getchar();
            switch (input) {
               case 'q':
                   exit(EXIT_SUCCESS);
               case 'r':
                   print_registers(pid);
                   break;
               case 's':
                   ptrace(PTRACE_SINGLESTEP, pid, NULL, 0);
               case 'c':
                   break;
               default:
                   goto redo;   //TODO: get rid of this shit somehow
            }
            ptrace(PTRACE_CONT, pid, NULL, 0);
        }
    }
}

int fork_to_child(int argc, char *argv[]) {
    pid_t childpid = fork();
    if (childpid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        char **args = malloc(sizeof(argv[0]) * (argc + 1));
        memcpy(args, argv, sizeof(argv[0]) * argc);
        args[argc] = NULL;
        execvp(args[0], args);
    }
    return childpid;
}
int print_registers(pid_t pid){
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS,pid,NULL,&regs);
    printf("rax: %x\trbx: %x",regs.rax,regs.rbx);
    printf("rcx: %x\trdx: %x",regs.rcx,regs.rdx);
    printf("rbp: %x\trsp: %x",regs.rbp,regs.rsp);
    printf("rsi: %x\trdi: %x",regs.rsi,regs.rdi);
    printf("r8: %x\tr9: %x",regs.r8,regs.r9);
    printf("r10: %x\tr11: %x",regs.r10,regs.r11);
    printf("r12: %x\tr13: %x",regs.r12,regs.r13);
    printf("r14: %x\tr15: %x",regs.r14,regs.r15);
}
