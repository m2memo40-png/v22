# Userland

`apps/hello.S` builds as a standalone ELF64 x86-64 static executable and is embedded into the kernel at `/bin/hello`. Use the kernel shell command `run /bin/hello` to exercise the current initial Ring-3 / `int 0x80` path after a successful boot.

The current ABI is documented in `docs/USERMODE_AR.md`: syscall numbers 0–4 are exit, write, getpid, yield and uptime. This is not POSIX; there is no libc, file-descriptor table, dynamic linker, `fork`/`exec`/`wait`, signal delivery, or command shell running in user mode. The runtime ring transition is compiled and kernel-self-tested but still needs an actual successful QEMU boot/run before being treated as verified.
