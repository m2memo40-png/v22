# Memory management interfaces

`pmm.h` exposes page allocation; `vmm.h` exposes kernel mappings and per-process address spaces (`vmm_space_create/map/unmap/translate`, user-memory copy, CR3 switch/root query). User data pages are owned by the ELF image/process layer, while VMM owns the page-table pages it creates.

No COW/demand paging or SMP-safe locking is implemented yet.
