# Tom's Debugger
This is a really simple debugger for 64-bit Linux systems. DWARF support soon™
```
Usage:
./tdb [options] program
  -s                    Begin program in single-step mode(same as setting breakpoint for first instruction)
  -b brktp1,brkpt2,...  Set a breakpoint at the specified address in the program
  -p [pid]              Instead of forking a new process, attach to an existing one(make sure your kernel allows this)
```
