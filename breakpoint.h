#ifndef BREAKPOINT_H
#define BREAKPOINT_H
typedef struct {
   long save;
   void *addr;
   int enabled;
}Breakpoint;
#endif
