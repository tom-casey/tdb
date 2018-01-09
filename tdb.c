#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include "tdb.h"
#include "breakpoint.h"
int fork_to_child(int argc, char *argv[]);
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [OPTION] \"program to debug\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *brkpt;
    pid_t pid = -1;
    int sflag = 0;
    int c;
    //TODO: allow "-"s in child arguments
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
    //split comma separated breakpoints into string array
    char **bps = NULL;
    if (brkpt) {
        printf("b argument is %s\n", brkpt);
        bps = str_split(brkpt, ',');
    }
    //if -p flag isn't used, try to spawn a child
    if (pid == -1) {
        pid = fork_to_child(argc - optind, &argv[optind]);
    }
    if (sflag == 1) {
        ptrace(PTRACE_SINGLESTEP, pid, NULL, 0);
    }
    //catch the first SIGTRAP upon attaching, we dont need to prompt
    int ss_status;
    waitpid(pid, &ss_status, 0);
    if (!WIFSTOPPED(ss_status)) {
        fprintf(stderr, "Failed to attach to proccess\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Attached to process %d sucessfully\n", pid);
    }
    Breakpoint **bplist = malloc(sizeof(Breakpoint *) * 256);
    if (bps) {
        int num_bpts = sizeof(bps) / sizeof(char *);
        if (num_bpts > 256) {
            fprintf(stderr, "Error: Too many breakpoints");
        }
        //for each breakpoint requested, create a struct instance for it and insert
        for (int i = 0; i < num_bpts; i++) {
            bplist[i] = malloc(sizeof(Breakpoint));
            sscanf(bps[i], "%x", &bplist[i]->addr);
            if (!(bplist[i]->addr)) {
                fprintf(stderr,
                        "%s is not a valid memory address. Skipping\n",
                        bps[i]);
                continue;
            }
            printf("Breakpoint to be set @ %p\n", bplist[i]->addr);
            insert_bp(bplist[i], pid);
        }
    }
    //now that we have our breakpoints in place we can start running the program
    ptrace(PTRACE_CONT, pid, NULL, 0);
    int status;
    //some form of a repl loop that waits for a breakpoint/sigtrap to prompt
    while (pid == waitpid(pid, &status, 0)) {
        if (WIFEXITED(status)) {
            printf("Child exited with status %d\n", WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status)) {
            printf("Child got killing signal %d\n", WTERMSIG(status));
        }
        if (WIFSTOPPED(status)) {
            printf("Child caught signal %d\n", WSTOPSIG(status));
          redo:
            puts("(q)uit\t(r)egisters\t(s)inglestep\t(c)ontinue\t(m)odify registers");
            char input;
            char *opt = { '\0' };
            unsigned long long int val;
            size_t line_size;
            char *line = NULL;
            getline(&line, &line_size, stdin);
            char *pos;
            //take out the \n at the end of the string
            if ((pos = strchr(line, '\n')) != NULL)
                *pos = '\0';
            //get our 3 arguments and place them in the proper places
            char *token;
            char *state;
            token = strtok_r(line, " ", &state);
            input = token[0];
            if (strlen(token) > 1) {
                goto redo;
            }
            token = strtok_r(NULL, " ", &state);
            if (token != NULL) {
                opt = malloc(strlen(token) + 1);
                strcpy(opt, token);
            }
            token = strtok_r(NULL, " ", &state);
            if (token != NULL) {
                if (sscanf(token, " %llx", &val) == 0) {
                    printf("Invalid value %s\n", token);
                    goto redo;
                }
            }
            fflush(stdin);
            switch (input) {
               case 'q':
                   //TODO: free alloc'd memory
                   exit(EXIT_SUCCESS);
               case 's':
                   ptrace(PTRACE_SINGLESTEP, pid, NULL, 0);
                   break;
               case 'c':
                   asm("nop");  //this makes the compiler happy
                   int num_breakpoints =
                       sizeof(bplist) / sizeof(Breakpoint *);
                   //acquire current register values
                   struct user_regs_struct regs;
                   ptrace(PTRACE_GETREGS, pid, NULL, &regs);
                   //see if we have any breakpoints enabled that are at pc-1
                   void *location = (void *) regs.rip - 1;
                   for (int i = 0; i < num_breakpoints; i++) {
                       if (bplist[i]->addr == location && bplist[i]->enabled) {
                           //set the pc back on the instruction we replaced with 0xCC
                           regs.rip = regs.rip - 1;
                           ptrace(PTRACE_SETREGS, pid, NULL, &regs);
                           //replace the modified instruction with the original
                           remove_bp(bplist[i], pid);
                           //step over it and catch the SIGTRAP
                           ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
                           waitpid(pid, NULL, 0);
                           //put the breakpoint back in
                           insert_bp(bplist[i], pid);
                           break;
                       }
                   }
                   //continue on our merry way
                   ptrace(PTRACE_CONT, pid, NULL, 0);
                   break;
               case 'm':
                   //if we dont have a register to modify, throw a fit
                   if (!opt) {
                       printf
                           ("Error invalid command: m [register] [value]\n");
                       goto redo;
                   }
                   if (modify_register(pid, opt, val) == -1) {
                       printf("Invalid register!\n");
                   }
                   goto redo;
               case 'r':
                   //if user specified a register, only print that one
                   if (opt) {
                       if (print_register(pid, opt) == -1) {
                           printf("Register not found\n");
                       }
                       goto redo;
                   }
                   print_all_registers(pid);
               default:
                   //loop back
                   goto redo;   //TODO: get rid of this shit somehow
            }
        }
    }
}

//create child process that gets ptraced
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

//print the requested register if it exists
int print_register(pid_t pid, char *reg) {
    //get all registers
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
    //request the address of the register in our user_regs_struct
    unsigned long long int *addr = register_to_address(&regs, reg);
    //check if we got a valid address back
    if (!addr) {
        printf("Register \"%s\" not found\n", reg);
        return -1;
    }
    printf("%s: %llx\n", reg, *addr);
}

//resolve a string of a register name to its address in a given referenced user_regs_struct
unsigned long long int *register_to_address(struct user_regs_struct *regs,
                                            char *string) {
    //ugly if chain(there's no reliable way to avoid this since we don't know exactly how things are aligned in the struct)
    if (strcmp(string, "r15") == 0)
        return &(regs->r15);
    if (strcmp(string, "r14") == 0)
        return &(regs->r14);
    if (strcmp(string, "r13") == 0)
        return &(regs->r13);
    if (strcmp(string, "r12") == 0)
        return &(regs->r12);
    if (strcmp(string, "rbp") == 0)
        return &(regs->rbp);
    if (strcmp(string, "rbx") == 0)
        return &(regs->rbx);
    if (strcmp(string, "r11") == 0)
        return &(regs->r11);
    if (strcmp(string, "r10") == 0)
        return &(regs->r10);
    if (strcmp(string, "r9") == 0)
        return &(regs->r9);
    if (strcmp(string, "r8") == 0)
        return &(regs->r8);
    if (strcmp(string, "rax") == 0)
        return &(regs->rax);
    if (strcmp(string, "rcx") == 0)
        return &(regs->rcx);
    if (strcmp(string, "rdx") == 0)
        return &(regs->rdx);
    if (strcmp(string, "rsi") == 0)
        return &(regs->rsi);
    if (strcmp(string, "rip") == 0)
        return &(regs->rip);
    if (strcmp(string, "eflags") == 0)
        return &(regs->eflags);
    if (strcmp(string, "rsp") == 0)
        return &(regs->rsp);
    else
        return NULL;
}

//print all registers
int print_all_registers(pid_t pid) {
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
#ifdef __x86_64__
    printf("rax: %llx\trbx: %llx\n", regs.rax, regs.rbx);
    printf("rcx: %llx\trdx: %llx\n", regs.rcx, regs.rdx);
    printf("r8: %llx\tr9: %llx\n", regs.r8, regs.r9);
    printf("r10: %llx\tr11: %llx\n", regs.r10, regs.r11);
    printf("r12: %llx\tr13: %llx\n", regs.r12, regs.r13);
    printf("r14: %llx\tr15: %llx\n", regs.r14, regs.r15);
    printf("rbp: %llx\trsp: %llx\n", regs.rbp, regs.rsp);
    printf("rsi: %llx\trdi: %llx\n", regs.rsi, regs.rdi);
    printf("rip: %llx\teflags: %llx\n", regs.rip, regs.eflags);
#else
    puts("Architecure not supported");
#endif
}
//modify a given register
int modify_register(pid_t pid, char *reg, unsigned long long int value) {
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
    //resolve our string to a register
    unsigned long long int *target = register_to_address(&regs, reg);
    if(!target){
        printf("Bad register name\n");
        return -1;
    }
    //send the modified version back to the kernel
    (*target) = value;
    ptrace(PTRACE_SETREGS, pid, NULL, &regs);
}
