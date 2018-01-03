#ifndef TDB_H
#define TDB_H
#include "breakpoint.h"
int insert_bp(Breakpoint *, pid_t);
char **str_split(char *, const char);
int print_registers(pid_t pid);
#endif
