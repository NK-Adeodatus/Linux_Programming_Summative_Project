section .data
    filename db "sensor_readings.txt", 0
    err_open_msg db "Error: Cannot open file", 10
    err_open_len equ $ - err_open_msg

section .bss
    ; Uninitialized data will go here (e.g., our file read buffer)

section .text
    global _start

_start:
    ; 1. Open the file
    mov rax, 2          ; sys_open system call number
    mov rdi, filename   ; pointer to the null-terminated filename string
    mov rsi, 0          ; flag: O_RDONLY (read-only access)
    mov rdx, 0          ; mode (not required when just opening an existing file)
    syscall             ; invoke the kernel

    ; 2. Check for error (file descriptor in rax will be < 0 if failed)
    cmp rax, 0
    jl .open_error

    ; (File descriptor is safe in rax, ready for reading in the next step)
    jmp .exit_success

.open_error:
    ; 3. Print error message
    mov rax, 1          ; sys_write system call number
    mov rdi, 1          ; stdout file descriptor
    mov rsi, err_open_msg ; pointer to the error message string
    mov rdx, err_open_len ; length of the error message
    syscall

    ; 4. Exit with error status
    mov rax, 60         ; sys_exit system call number
    mov rdi, 1          ; exit status 1 (error)
    syscall

.exit_success:
    ; Exit the program cleanly
    mov rax, 60         ; sys_exit system call number
    mov rdi, 0          ; exit status 0 (success)
    syscall             ; invoke the kernel
