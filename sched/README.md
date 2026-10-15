# Scheduler and process records

`scheduler_core.c` provides single-CPU preemptive scheduling from the PIT interrupt, per-task kernel stacks, saved interrupt frames, CR3 selection and TSS RSP0 switching. `process.c` provides PID/state records, kernel-process startup, the initial Ring-3 process context, exit and reap.

Limits: this is not a POSIX process model; no fork/wait/signals, SMP, robust priority scheduler or hardened lifecycle exists. Ring-3 execution must be verified through a successful QEMU run.
