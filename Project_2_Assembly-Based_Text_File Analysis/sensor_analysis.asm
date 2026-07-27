section .data
    ; Initialized data will go here (e.g., file name, output strings)

section .bss
    ; Uninitialized data will go here (e.g., our file read buffer)

section .text
    global _start

_start:
    ; Exit the program cleanly
    mov rax, 60         ; sys_exit system call number
    mov rdi, 0          ; exit status 0 (success)
    syscall             ; invoke the kernel
