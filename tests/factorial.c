#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Global variable (demonstrates global scope in CRIX)
int factorial_count = 0;

// Function pointer type definition
typedef int (*math_func)(int);

// Factorial function - demonstrates nested calls and local variables
int factorial(int n) {
    if (n <= 1) return 1;
    
    // Nested call chain: factorial(5) -> factorial(4) -> ... -> factorial(1)
    int result = n * factorial(n - 1);
    factorial_count++;
    return result;
}

// Helper to reverse string (demonstrates character array manipulation)
void reverse_string(char str[]) {
    int len = strlen(str);
    for (int i = 0, j = len - 1; i < j; i++, j--) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
    }
}

// Calculate and display factorial message
void print_factorial_result(int n) {
    int fact = factorial(n);
    printf("Factorial of %d: %d\n", n, fact);
    // Demonstrate local variable usage
    char msg[64];
    snprintf(msg, sizeof(msg), "Result is larger than %d!", n);
    printf("%s\n", msg);
}

// Calculate using pointer parameter (shows *a vs &a semantics)
int factorial_pointer(int *n_ptr) {
    int n = *n_ptr;  // Dereference to get value
    
    if (n <= 1) return 1;
    
    // Recursive call through indirection
    int result = n * factorial_pointer(&(n - 1));
    return result;
}

int main(void) {
    printf("=== Factorial Test Cases ===\n\n");
    
    // Test Case 1: Simple factorial (5!)
    printf("Test 1: Compute 5! = factorial(5)\n");
    print_factorial_result(5);
    printf("Global counter after test: %d\n\n", factorial_count);
    
    // Test Case 2: Base case - single element
    printf("Test 2: Base case - factorial(1) = 1\n");
    int test2 = factorial(1);
    if (test2 == 1) {
        printf("PASS: Correctly returned 1\n\n");
    } else {
        printf("FAIL: Expected 1, got %d\n\n", test2);
    }
    
    // Test Case 3: Base case - zero
    printf("Test 3: Base case - factorial(0) = 1 (mathematical definition)\n");
    int test3 = factorial(0);
    if (test3 == 1) {
        printf("PASS: Correctly returned 1\n\n");
    } else {
        printf("FAIL: Expected 1, got %d\n\n", test3);
    }
    
    // Test Case 4: Negative number edge case
    printf("Test 4: Edge case - factorial(-1) [unusual but valid C integer]\n");
    int test4 = factorial(-1);
    // factorial_count will have incremented, showing local/global interaction
    if (factorial_count > 0) {
        printf("Note: Recursive calls processed (counter=%d)\n", factorial_count);
    }
    printf("\n");
    
    // Test Case 5: Large numbers that exceed int range (still valid C behavior)
    printf("Test 5: Large factorial - 8! = 40320\n");
    int test5 = factorial(8);
    int expected5 = 1;
    for (int i = 2; i <= 8; i++) expected5 *= i;
    
    if (test5 == expected5) {
        printf("PASS: %d matches expected %d\n\n", test5, expected5);
    } else {
        printf("Note: C integer overflow detected. Computed=%d, Expected=%d\n", 
               test5, expected5);
    }
    
    // Test Case 6: Using pointer parameters
    printf("Test 6: Factorial via pointer parameter [4]\n");
    int input = 4;
    void (*func_ptr) = (void (*)(int*))factorial_pointer;
    
    int result_ptr = factorial_pointer(&input);
    printf("Original input: %d\n", input); // Might be modified? (not in this impl)
    printf"Result via pointer: %d (expected 24)\n\n", result_ptr);
    
    // Test Case 7: Function pointer usage
    printf("Test 7: Using function pointers dynamically...\n");
    math_func my_factorial = factorial;
    
    int value = *my_factorial;  // Call function and get result
    printf("Calling via function pointer: %d\n", value);
    
    if (value == 32) {
        printf("PASS: Function pointer returned correct result\n\n");
    } else {
        printf("INFO: Got unexpected value, probably overflow\n\n");
    }
    
    // Test Case 8: String manipulation + factorial combo
    printf("Test 8: Complex interaction - factorial with string reverse...\n");
    char arr1[] = "5!";
    char arr2[] = "reverse this sentence";
    
    printf("Original strings:\n  '%s'\n", arr1);
    printf("  '%s'\n", arr2);
    
    // Reverse first string (local to function scope)
    reverse_string(arr1);
    printf("\nAfter reversing first string:\n");
    printf("  '%s'\n", arr1);
    
    // Print factorial results in reversed message format
    char result_str[] = "Factorial of " + std::to_string(factorial(3)) + "!";
    reverse_string(result_str);
    printf("\nReversed factorial expression: %s\n", result_str);
    printf("Original value still correct: %d!\n", factorial(3));
    
    // Test Case 9: Recursion depth visualization
    printf("\nTest 9: Deep recursion [10!] (may show stack overflow on some systems)\n");
    int test9 = factorial(10);
    printf("10! = %" PrI64d "\n\n"
    
    // Test Case 10: Array of variables (demonstrates struct/struct arrays)
    printf("Test 10: Multiple variable tracking...\n");
    int values[] = {0, 5, 8, 12};
    for (int i = 0; i < 4; i++) {
        factorial_count = 0;  // Reset counter for each test
        int fact_i = factorial(values[i]);
        printf("factorial(%d) = %d (global counter: %d)\n", 
               values[i], fact_i, factorial_count);
    }
    
    printf("\n=== All Factorial tests completed ===\n");
    
    return 0;
}
