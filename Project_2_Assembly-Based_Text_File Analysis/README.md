# Project 2: Assembly-Based Text File Analysis

This project is an x86-64 assembly program that reads a text file (`sensor_readings.txt`), parses it byte-by-byte in memory, and accurately counts the total number of lines as well as the number of lines containing valid (non-empty) sensor data. It correctly handles both Unix (`\n`) and Windows (`\r\n`) line endings.

## Prerequisites
- Linux environment (or WSL on Windows)
- `nasm` (Netwide Assembler)
- `ld` (GNU Linker)

## Build Instructions

1. Open your terminal and navigate to the project directory.

2. Assemble the source code into a 64-bit ELF object file:
   ```bash
   nasm -f elf64 sensor_analysis.asm -o sensor_analysis.o
   ```
3. Link the object file to create the final executable:
   ```bash
   ld sensor_analysis.o -o sensor_analysis
   ```

## Run Instructions

1. Ensure the `sensor_readings.txt` file is in the same directory as the executable.
2. Run the program:
   ```bash
   ./sensor_analysis
   ```

## Expected Output (based on provided sample file)
```text
Total records: 7
Valid records: 4
```
