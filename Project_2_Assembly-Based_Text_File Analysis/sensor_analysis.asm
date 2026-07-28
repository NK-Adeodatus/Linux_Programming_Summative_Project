section .data
    filename db "sensor_readings.txt", 0
    err_open_msg db "Error: Cannot open file", 10
    err_open_len equ $ - err_open_msg
    err_read_msg db "Error: Cannot read file", 10
    err_read_len equ $ - err_read_msg
    msg_total db "Total records: "
    len_total equ $ - msg_total
    msg_valid db "Valid records: "
    len_valid equ $ - msg_valid
    newline db 10

section .bss
    buffer resb 4096    ; reserve 4096 bytes for file content
    num_buffer resb 20  ; buffer to hold the ASCII digits for printing

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
    mov r12, rax        ; save file descriptor in r12

    ; 3. Read the file
    mov rax, 0          ; sys_read system call number
    mov rdi, r12        ; file descriptor
    mov rsi, buffer     ; pointer to our memory buffer
    mov rdx, 4096       ; maximum number of bytes to read
    syscall

    ; 4. Check for error (bytes read in rax < 0)
    cmp rax, 0
    jl .read_error

    ; Save the number of bytes read in r13 for traversal later
    mov r13, rax

    ; 5. Close the file
    mov rax, 3          ; sys_close system call number
    mov rdi, r12        ; file descriptor to close
    syscall

    ; 6 & 7. Traversal Loop, Total Line Counting & Valid Record Detection
    xor r14, r14        ; r14 = 0 (buffer index)
    xor r15, r15        ; r15 = 0 (total_lines counter)
    xor r12, r12        ; r12 = 0 (valid_lines counter) ; CHANGED to r12 (callee-saved)
    xor r8, r8          ; r8  = 0 (current line has data flag)

    cmp r13, 0          ; if bytes read == 0, file is empty
    je .print_results

.traverse_loop:
    cmp r14, r13        ; have we processed all bytes?
    jge .check_last_line

    mov al, byte [buffer + r14] ; load 1 byte into AL

    cmp al, 10          ; is it a Unix newline (\n, ASCII 10)?
    je .handle_newline

    cmp al, 13          ; is it a Windows carriage return (\r, ASCII 13)?
    je .next_byte       ; if yes, just ignore it

    ; It's neither \n nor \r, so it's actual data
    mov r8, 1           ; set flag: current line has data
    jmp .next_byte

.handle_newline:
    inc r15             ; increment total_lines counter

    cmp r8, 1           ; did this line have valid data?
    jne .reset_flag
    inc r12             ; if yes, increment valid_lines counter

.reset_flag:
    xor r8, r8          ; reset flag to 0 for the next line

.next_byte:
    inc r14             ; increment buffer index
    jmp .traverse_loop

.check_last_line:
    ; If the file doesn't end with a newline, the last line wasn't counted.
    mov r14, r13        ; get total bytes read
    dec r14             ; point to the very last byte (index = length - 1)
    mov al, byte [buffer + r14]
    cmp al, 10          ; was the last byte a newline?
    je .print_results   ; if yes, we already handled it in the loop

    inc r15             ; if not, increment total_lines for the final line
    cmp r8, 1           ; did this final line have data?
    jne .print_results
    inc r12             ; if yes, increment valid_lines for the final line

.print_results:
    ; 8. Output the results
    ; Print "Total records: "
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout
    mov rsi, msg_total
    mov rdx, len_total
    syscall

    ; Print total_lines (r15)
    mov rax, r15
    call print_number

    ; Print newline
    mov rax, 1
    mov rdi, 1
    mov rsi, newline
    mov rdx, 1
    syscall

    ; Print "Valid records: "
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_valid
    mov rdx, len_valid
    syscall

    ; Print valid_lines (r12)
    mov rax, r12
    call print_number

    ; Print newline
    mov rax, 1
    mov rdi, 1
    mov rsi, newline
    mov rdx, 1
    syscall

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

.read_error:
    ; Print read error message
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout
    mov rsi, err_read_msg
    mov rdx, err_read_len
    syscall

    ; Exit with error status
    mov rax, 60
    mov rdi, 1
    syscall

.exit_success:
    ; Exit the program cleanly
    mov rax, 60         ; sys_exit system call number
    mov rdi, 0          ; exit status 0 (success)
    syscall             ; invoke the kernel

; Subroutine: print_number
; Converts an integer in rax to a string and prints it
print_number:
    mov rcx, 0          ; digit counter
    mov rbx, 10         ; divisor (we want base 10)
    
.divide_loop:
    xor rdx, rdx        ; clear rdx before division (rdx:rax / rbx)
    div rbx             ; rax = quotient, rdx = remainder
    add rdx, '0'        ; convert remainder to ASCII character (add 48)
    push rdx            ; push digit onto stack (digits are calculated right-to-left)
    inc rcx             ; increment digit count
    cmp rax, 0          ; is quotient zero?
    jne .divide_loop    ; if not, keep dividing

.print_digits:
    pop rdx             ; pop digit from stack (reverses the order to left-to-right)
    mov byte [num_buffer], dl ; store in our output buffer
    
    ; preserve rcx (it gets clobbered by syscall)
    push rcx
    
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout
    mov rsi, num_buffer ; pointer to digit
    mov rdx, 1          ; length 1
    syscall
    
    pop rcx             ; restore rcx
    dec rcx             ; decrement digit count
    cmp rcx, 0
    jne .print_digits
    
    ret                 ; return to main code
