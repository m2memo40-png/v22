.section .rodata.fluxmax_builtin_hello,"a",@progbits
.balign 16
.global fluxmax_builtin_hello_elf_start
.global fluxmax_builtin_hello_elf_end
fluxmax_builtin_hello_elf_start:
    .incbin "build/userland/hello.elf"
fluxmax_builtin_hello_elf_end:
.section .note.GNU-stack,"",@progbits
