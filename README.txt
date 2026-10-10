SYSC 4001 Assignment 1 - README
Isabella Hernandez, Student number: 101352857

BUILD AND RUN
  make clean && make
  ./scheduler -i <input.csv> -a <fcfs|rr|srtf> -q <quantum> -c <context_switch> -o <output_dir>
  python3 test_harness.py   (all 7 public tests pass)

DESIGN
- All simulator code is in src/student_scheduler.c.
- Each process has a PCB holding its ProcessSpec, state, remaining CPU time,
  bursts completed, I/O finish time, and RR quantum used.
- The Ready queue is a circular array of PCB indices (enqueue/dequeue).
  Waiting processes are kept in the PCB array and checked each tick.
- A CpuState struct tracks whether the CPU is idle, context switching, or running.
- The main loop follows the handout order each tick: resolve CPU interval,
  admit arrivals, complete I/O, SRTF preemption check, dispatch/execute.

KEY IMPLEMENTATION DETAILS
- The next process is chosen when a context switch finishes, not when it
  starts, so arrivals during the switch are considered.
- With context-switch cost 0, a process is dispatched in the same tick.
- When I/O completes, remaining CPU time is reset to the next full burst, so
  SRTF compares correct values.
- SRTF ties go to the process that entered the Ready queue first. Preemption
  only happens when remaining time is strictly less.
- RR: a burst that ends exactly at the quantum boundary completes normally.

AI USE
I used Claude as a tutor and debugging helper
while working on this assignment.

What I asked for help:
- Helper Functions: I asked Claude to explain to me in depth what the helper functions are inteded for and how they 
would be used while the code was running so my understanding for them would strength and help me write the functions
- Explinations: Asking it for advice on how to procced next after a step was finished and I felt stuck. 
- Debugging: When an error arose I attempted to debug it but if i foudn it was taking up too much of my time 
I would as Claude to give me hints and guide me towards trying to still solve it on my own but with help
- Report: Claude drafted example explanations and a layout from my
simulator output. I wrote the final text myself and created the tables. I used claudes example as a 
template on how to format it and the length of my writing. 