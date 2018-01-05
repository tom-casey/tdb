#ifndef TDB_H
#define TDB_H
#include "breakpoint.h"
int insert_bp(Breakpoint *, pid_t);
int remove_bp(Breakpoint *, pid_t);
char **str_split(char *, const char);
int print_all_registers(pid_t pid);
unsigned long long int* register_to_address(struct user_regs_struct * regs, char* string);
int print_register(pid_t,char*);
int modify_register(pid_t pid, char* reg,unsigned long long int value);
#endif
