# Kernel scheduler and process interfaces

Kernel threads and an initial user task context are implemented in source. Process records hold PID, address-space pointer, entry and user stack; shell's `run PATH` loads a static ELF64 user program and waits for exit.

This remains an early implementation with no fork/wait/signals, no multicore safety and no claim of production security. See `docs/USERMODE_AR.md`.
