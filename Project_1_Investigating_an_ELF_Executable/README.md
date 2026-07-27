# Project 1: Investigating an ELF Executable

## Overview
This project involves writing a small C application and performing a detailed investigation of how it is represented and executed as an ELF (Executable and Linkable Format) binary on a Linux system. The analysis covers static analysis, dynamic analysis, and interactive debugging using standard Linux tools.

## Files
| File | Description |
|------|-------------|
| `program.c` | The C source code for the application |
| `program` | The compiled and stripped ELF executable (x86-64, Linux) |
| `analysis_report.md` | The concise analysis report containing all observations and explanations |

## Program Structure
The C program (`program.c`) was deliberately designed to contain structures that are rich for reverse engineering analysis:

- **Global Variable:** `global_counter` — an initialized global integer, stored in the `.bss` section of the ELF binary.
- **Function 1 — `allocate_memory()`:** Dynamically allocates an integer array on the heap using `malloc()`, with a `NULL` check for safety.
- **Function 2 — `process_data()`:** Fills the array using a `for` loop and an `if-else` conditional, and increments the global counter on each iteration.
- **Function 3 — `print_and_cleanup()`:** Prints the array contents and global counter to the terminal using `printf()`, then frees the allocated memory using `free()`.
- **`main()`:** Orchestrates the three functions in sequence with an array size of 5.

## How to Compile and Run
These commands must be run inside a Linux environment (e.g., WSL, a VM, or a Linux machine):

```bash
# Compile with no optimization and no inlining
gcc -Wall -O0 -fno-inline -o program program.c

# Strip debugging and symbol information
strip program

# Run the program
./program
```

## Expected Output
```
Processed Array Data:
Index 0: 0
Index 1: 3
Index 2: 4
Index 3: 9
Index 4: 8
Global counter value: 5
Memory successfully freed.
```

## Tools Used for Analysis
| Tool | Purpose |
|------|---------|
| `readelf` | Inspecting ELF headers and section information |
| `objdump` | Disassembling the `.text` section for static analysis |
| `ldd` | Verifying dynamic linking dependencies |
| `strace` | Tracing system calls during execution |
| `gdb` | Interactive debugging, breakpoints, and memory inspection |
