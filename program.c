#include <stdio.h>
#include <stdlib.h>

// Global variable as required by the assignment
int global_counter = 0;

// Function 1: Dynamically allocates memory
int* allocate_memory(int size) {
    int *array = (int *)malloc(size * sizeof(int));
    if (array == NULL) {
        printf("Memory allocation failed!\n");
        exit(1);
    }
    return array;
}

// Function 2: Processes data using a loop and conditional logic
void process_data(int *array, int size) {
    for (int i = 0; i < size; i++) {
        if (i % 2 == 0) {
            array[i] = i * 2;
        } else {
            array[i] = i * 3;
        }
        global_counter++;
    }
}

// Function 3: Displays output and frees memory
void print_and_cleanup(int *array, int size) {
    printf("Processed Array Data:\n");
    for (int i = 0; i < size; i++) {
        printf("Index %d: %d\n", i, array[i]);
    }
    printf("Global counter value: %d\n", global_counter);
    
    free(array);
    printf("Memory successfully freed.\n");
}

int main(void) {
    int size = 5; // Small size for manageable output
    
    // Call Function 1
    int *my_data = allocate_memory(size);
    
    // Call Function 2
    process_data(my_data, size);
    
    // Call Function 3
    print_and_cleanup(my_data, size);
    
    return 0;
}
