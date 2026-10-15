# User applications

`hello.S` is the demonstration ELF64 x86-64 static user program, built as `build/userland/hello.elf` and embedded in the kernel VFS at `/bin/hello`. It prints via syscall `write` and exits with syscall `exit`; use `run /bin/hello` after boot. Runtime must be checked in QEMU before relying on the Ring-3 path.
