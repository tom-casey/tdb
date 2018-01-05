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
               fprintf(stderr, "Bad option -%c\n", optopt);
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
    }
    if (sflag == 1) {
        ptrace(PTRACE_SINGLESTEP, pid, NULL, 0);
    }
    int ss_status;
    waitpid(pid, &ss_status, 0);
    if(!WIFSTOPPED(ss_status)){
        fprintf(stderr, "Failed to attach to proccess\n");
        exit(EXIT_FAILURE);
    }
    else{
        printf("Attached to process %d sucessfully\n",pid);
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
            printf("Breakpoint to be set @ %x\n",bplist[i]->addr);
            insert_bp(bplist[i], pid);
        }
    }
    ptrace(PTRACE_CONT, pid, NULL, 0);
    int status;
    while (pid == waitpid(pid, &status, 0)) {
        if (WIFEXITED(status)) {
            printf("Child exited with status %d\n", WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status)) {
            printf("Child got killing signal %d\n", WTERMSIG(status));
        }
        if (WIFSTOPPED(status)) {
            printf("Child caught signal %d\n",WSTOPSIG(status));
          redo:
            puts("(q)uit\t(r)egisters\t(s)inglestep\t(c)ontinue");
            char input;
            scanf(" %c",&input);
            fflush(stdin);
            switch (input) {
               case 'q':
                   exit(EXIT_SUCCESS);
               case 's':
                   ptrace(PTRACE_SINGLESTEP, pid, NULL, 0);
                   break;
               case 'c':
                   ptrace(PTRACE_CONT, pid, NULL, 0);
                   break;
               case 'r':
                   print_registers(pid);
               default:
                   goto redo;   //TODO: get rid of this shit somehow
            }
        }
    }
}

int fork_to_child(int argc, char *argv[]) {
    pid_t childpid = fork();
    ptrace(PTRACE_SETOPTIONS, childpid, NULL, PTRACE_O_TRACEEXEC);
    if (childpid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        char **args = malloc(sizeof(argv[0]) * (argc + 1));
        memcpy(args, argv, sizeof(argv[0]) * argc);
        args[argc] = NULL;
        execv(args[0], args);
    }
    return childpid;
}
int print_registers(pid_t pid){
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS,pid,NULL,&regs);
    #ifdef __x86_64__
    printf("rax: %x\trbx: %x\n",regs.rax,regs.rbx);
    printf("rcx: %x\trdx: %x\n",regs.rcx,regs.rdx);
    printf("rbp: %x\trsp: %x\n",regs.rbp,regs.rsp);
    printf("rsi: %x\trdi: %x\n",regs.rsi,regs.rdi);
    printf("r8: %x\tr9: %x\n",regs.r8,regs.r9);
    printf("r10: %x\tr11: %x\n",regs.r10,regs.r11);
    printf("r12: %x\tr13: %x\n",regs.r12,regs.r13);
    printf("r14: %x\tr15: %x\n",regs.r14,regs.r15);
    printf("rip: %x\t\n",regs.rip);
    #else
    puts("Architecure not supported");
    #endif
}
