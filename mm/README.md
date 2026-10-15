# Memory management

`pmm.c` initializes a page allocator from Limine's usable-memory ranges. `heap.c` provides a fixed kernel heap. `vmm.c` handles canonical x86-64 mappings and per-process address-space roots whose user page-table paths are privately copied/split as needed. It includes translation and checked kernel-side copy helpers for user pointers, with NX support when advertised by the CPU.

This remains early-stage memory management: the PMM list is simple, kernel heap is fixed-size, no demand paging/COW exists, page-table code has not had a full runtime/fuzz/security review, and SMP locking is absent. Use `make test` for host tests and `selftest` in a successfully booted emulator for the in-kernel mapping/ELF checks.
