.section .text
.global _start
.type _start,@function
_start:
    mov $1, %rax                 /* FLUXMAX_SYS_WRITE */
    mov $1, %rdi                 /* stdout */
    lea message(%rip), %rsi
    mov $(message_end-message), %rdx
    int $0x80

    mov $2, %rax                 /* FLUXMAX_SYS_GETPID */
    int $0x80
    /* Keep this demo output deterministic; exit status 0 means success. */
    xor %rdi, %rdi
    xor %rax, %rax                /* FLUXMAX_SYS_EXIT */
    int $0x80
1:  ud2
    jmp 1b
.size _start, .-_start

.section .rodata
message:
    .ascii "Hello from Fluxmax user mode (Ring 3)!\n"
message_end:
.section .note.GNU-stack,"",@progbits
