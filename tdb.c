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
        exit(EXIT_FAILURE);
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
            puts("(q)uit\t(r)egisters\t(s)inglestep\t(c)ontinue\t(m)odify registers");
            char input;
            char* opt;
            unsigned long long int val;
            scanf(" %c %s %d",&input,&opt,&val);
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
               case 'm':
                   if(!opt||!value){
                       printf("Error invalid command: m [register] [value]\n");
                       goto redo;
                   }
                   if(modify_register(pid,opt,val)==-1){
                       printf("Invalid register!\n");
                       goto redo;
                   }
               case 'r':
                   if(opt){
                       print_register(opt);
                   }
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
    printf("r8: %x\tr9: %x\n",regs.r8,regs.r9);
    printf("r10: %x\tr11: %x\n",regs.r10,regs.r11);
    printf("r12: %x\tr13: %x\n",regs.r12,regs.r13);
    printf("r14: %x\tr15: %x\n",regs.r14,regs.r15);
    printf("rbp: %x\trsp: %x\n",regs.rbp,regs.rsp);
    printf("rsi: %x\trdi: %x\n",regs.rsi,regs.rdi);
    printf("rip: %x\teflags: %x\n",regs.rip,regs.eflags);
    #else
    puts("Architecure not supported");
    #endif
}
int modify_register(pid_t pid, char* reg,unsigned long long int value){
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS,pid,NULL,&regs);
    unsigned long long int* target = NULL;
    switch(reg){
        case "r15":
            target = &regs.r15
            break;
        case "r14":
            target = &regs.r14
            break;
        case "r13":
            target = &regs.r13
            break;
        case "r12":
            target = &regs.r12
            break;
        case "rbp":
            target = &regs.rbp
            break;
        case "rbx":
            target = &regs.rbx
            break;
        case "r11":
            target = &regs.r11
            break;
        case "r10":
            target = &regs.r10
            break;
        case "r9":
            target = &regs.r9
            break;
        case "r8":
            target = &regs.r8
            break;
        case "rax":
            target = &regs.rax
            break;
        case "rcx":
            target = &regs.rcx
            break;
        case "rdx":
            target = &regs.rdx
            break;
        case "rsi":
            target = &regs.rsi
            break;
        case "rip":
            target = &regs.rip
            break;
        case "eflags":
            target = &regs.eflags
            break;
        case "rsp":
            target = &regs.r15
            break;
        default:
            return -1;
        *target = value;
        ptrace(PTRACE_SETREGS,pid,NULL,&regs);
    }
}
