#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_SIZE 100

int binary_search(const int arr[], int size, int target) {
    int left = 0;
    int right = size - 1;
    int result = -1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        
        if (arr[mid] == target) {
            result = mid;
            break;
        } else if (arr[mid] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return result;
}

void print_array(const int arr[], int size) {
    printf("[");
    for (int i = 0; i < size; i++) {
        if (i > 0) printf(", ");
        printf("%d", arr[i]);
    }
    printf("]");
}

int main(void) {
    // Test Case 1: Standard binary search (sorted, unique)
    int test1[] = {1, 3, 5, 7, 9, 11, 13, 15};
    printf("Test 1: Binary search value 9 in [1,3,5,7,9,11,13,15]\n");
    int idx1 = binary_search(test1, 8, 9);
    printf("Result: index=%d (expected 4)\n\n", idx1);
    
    // Test Case 2: Edge case - first or last element
    printf("Test 2: Binary search value 1 at START of [1,3,5,7,9,11,13,15]\n");
    int idx2 = binary_search(test1, 8, 1);
    printf("Result: index=%d (expected 0)\n\n", idx2);
    
    // Test Case 3: Edge case - not found
    printf("Test 3: Binary search value 20 NOT in [1,3,5,7,9,11,13,15]\n");
    int idx3 = binary_search(test1, 8, 20);
    printf("Result: index=%d (expected -1)\n\n", idx3);
    
    // Test Case 4: Duplicates in array
    printf("Test 4: Binary search value 5 with DUPLICATES [1,3,5,5,5,7,9,11]\n");
    int test4[] = {1, 3, 5, 5, 5, 7, 9, 11};
    int idx4 = binary_search(test4, 8, 5);
    printf("Result: index=%d (will find one of the 5s)\n\n", idx4);
    
    // Test Case 5: Single element found
    printf("Test 5: Binary search in single-element array [42]\n");
    int test5[] = {42};
    int idx5 = binary_search(test5, 1, 42);
    printf("Result: index=%d (expected 0)\n\n", idx5);
    
    // Test Case 6: Empty consideration - array of size 0
    // Note: our binary_search handles size=0 by immediately returning -1
    printf("Test 6: Binary search with size=0 would need special handling\n");
    int test6[] = {};
    if (test6 == NULL || sizeof(test6)/sizeof(int) == 0) {
        printf("Special case detected: empty array\n");
    }
    
    // Test Case 7: Larger dataset for visualization
    printf("Test 7: Larger sorted array with value at middle...\n");
    int test7[] = {10, 20, 30, 40, 50, 60, 70, 80};
    int idx7 = binary_search(test7, 8, 50);
    print_array(test7, 8);
    printf("\nResult: index=%d (expected 4)\n\n", idx7);
    
    // Test Case 8: Reverse sorted array
    printf("Test 8: Binary search in REVERSE sorted array [15,13,11,9,7,5,3,1]\n");
    int test8[] = {15, 13, 11, 9, 7, 5, 3, 1};
    int idx8 = binary_search(test8, 8, 9);
    print_array(test8, 8);
    printf("\nResult: index=%d (expected 3, but algorithm assumes sorted!)\n", idx8);
    
    // Test Case 9: String literal as array name (C quirk)
    char arr_name[] = "test_data";
    printf("Test 9: Using array name in declaration:\n");
    int test9[] = {1, 2, 3, 4, 5};
    printf("Array declared: %s\n", arr_name);
    int idx9 = binary_search(test9, 5, 3);
    printf("Search for 3: index=%d (expected 2)\n\n", idx9);
    
    printf("=== All tests completed ===\n");
    
    return 0;
}
