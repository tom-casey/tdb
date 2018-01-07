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
        printf("b argument is %s\n", brkpt);
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
            if ((pos = strchr(line, '\n')) != NULL)
                *pos = '\0';
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
                   exit(EXIT_SUCCESS);
               case 's':
                   ptrace(PTRACE_SINGLESTEP, pid, NULL, 0);
                   break;
               case 'c':
                   asm("nop");  //C is shit and this makes the compiler happy
                   int num_breakpoints =
                       sizeof(bplist) / sizeof(Breakpoint *);
                   struct user_regs_struct regs;
                   ptrace(PTRACE_GETREGS, pid, NULL, &regs);
                   void *location = (void *) regs.rip - 1;
                   for (int i = 0; i < num_breakpoints; i++) {
                       if (bplist[i]->addr == location && bplist[i]->enabled) {
                           regs.rip = regs.rip - 1;
                           ptrace(PTRACE_SETREGS, pid, NULL, &regs);
                           remove_bp(bplist[i], pid);
                           ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
                           waitpid(pid, NULL, 0);
                           insert_bp(bplist[i], pid);
                           break;
                       }
                   }
                   ptrace(PTRACE_CONT, pid, NULL, 0);
                   break;
               case 'm':
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
                   if (opt) {
                       if (print_register(pid, opt) == -1) {
                           printf("Register not found\n");
                       }
                       goto redo;
                   }
                   print_all_registers(pid);
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
int print_register(pid_t pid, char *reg) {
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
    unsigned long long int *addr = register_to_address(&regs, reg);
    if (!addr) {
        printf("Register \"%s\" not found\n", reg);
        return -1;
    }
    printf("%s: %llx\n", reg, *addr);
}
unsigned long long int *register_to_address(struct user_regs_struct *regs,
                                            char *string) {
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
int modify_register(pid_t pid, char *reg, unsigned long long int value) {
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
    unsigned long long int *target = register_to_address(&regs, reg);
    (*target) = value;
    ptrace(PTRACE_SETREGS, pid, NULL, &regs);
}
