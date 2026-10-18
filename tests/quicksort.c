#include <stdio.h>
#include <stdlib.h>

// Swap two elements (pointer manipulation example)
void swap(int arr[], int i, int j) {
    int temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
}

// Partition function - classic QuickSort algorithm
int partition(int arr[], int low, int high) {
    int pivot = arr[high];
    int i = low - 1;  // Index of smaller element
    
    for (int j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i++;
            swap(arr, i, j);
        }
    }
    
    // Place pivot in correct position
    swap(arr, i + 1, high);
    return i + 1;
}

// Recursive QuickSort implementation
void quicksort(int arr[], int low, int high) {
    if (low < high) {
        // pi is partition index after partition call
        int pi = partition(arr, low, high);
        
        // Recursively sort elements before and after pivot
        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }
}

void print_array(const int arr[], int size) {
    printf("[");
    for (int i = 0; i < size; i++) {
        if (i > 0) printf(", ");
        printf("%d", arr[i]);
    }
    printf("]");
}

void print_trace(const char msg[]) {
    printf("%s\n", msg);
}

int main(void) {
    // Test Case 1: Standard QuickSort (random-ish data)
    printf("Test 1: QuickSort on [64,34,25,12,22,11,90]\n");
    int test1[] = {64, 34, 25, 12, 22, 11, 90};
    print_trace("Before sort: ");
    print_array(test1, sizeof(test1)/sizeof(int));
    int size1 = sizeof(test1)/sizeof(int);
    quicksort(test1, 0, size1 - 1);
    print_trace("After sort:   ");
    print_array(test1, size1);
    printf("\n\n");
    
    // Test Case 2: Already sorted array (best case for QuickSort)
    printf("Test 2: QuickSort on ALREADY SORTED [1,2,3,4,5,6,7,8]\n");
    int test2[] = {1, 2, 3, 4, 5, 6, 7, 8};
    print_trace("Before sort: ");
    print_array(test2, sizeof(test2)/sizeof(int));
    quicksort(test2, 0, sizeof(test2)/sizeof(int) - 1);
    print_trace("After sort:   ");
    print_array(test2, sizeof(test2)/sizeof(int));
    printf("(Should be the same)\n\n");
    
    // Test Case 3: Reverse sorted array (worst case for naive QuickSort)
    printf("Test 3: QuickSort on REVERSE SORTED [8,7,6,5,4,3,2,1]\n");
    int test3[] = {8, 7, 6, 5, 4, 3, 2, 1};
    print_trace("Before sort: ");
    print_array(test3, sizeof(test3)/sizeof(int));
    quicksort(test3, 0, sizeof(test3)/sizeof(int) - 1);
    print_trace("After sort:   ");
    print_array(test3, sizeof(test3)/sizeof(int));
    printf("(Performance degradation visible here!)\n\n");
    
    // Test Case 4: Small array (edge case)
    printf("Test 4: QuickSort on small array [5]\n");
    int test4[] = {5};
    quicksort(test4, 0, sizeof(test4)/sizeof(int) - 1);
    print_trace("After sort:   ");
    print_array(test4, sizeof(test4)/sizeof(int));
    printf("\n");
    
    // Test Case 5: Two elements
    printf("Test 5: QuickSort on [20,10]\n");
    int test5[] = {20, 10};
    quicksort(test5, 0, sizeof(test5)/sizeof(int) - 1);
    print_trace("After sort:   ");
    print_array(test5, sizeof(test5)/sizeof(int));
    printf("\n");
    
    // Test Case 6: Negative numbers and mixed types
    printf("Test 6: QuickSort with NEGATIVE numbers [-5,-2,0,3,-10,7]\n");
    int test6[] = {-5, -2, 0, 3, -10, 7};
    print_trace("Before sort: ");
    print_array(test6, sizeof(test6)/sizeof(int));
    quicksort(test6, 0, sizeof(test6)/sizeof(int) - 1);
    print_trace("After sort:   ");
    print_array(test6, sizeof(test6)/sizeof(int));
    printf("\n");
    
    // Test Case 7: Uniqueness check after sorting
    printf("Test 7: Verify no duplicates exist [3,1,4,1,5,9,2,6]\n");
    int test7[] = {3, 1, 4, 1, 5, 9, 2, 6};
    quicksort(test7, 0, sizeof(test7)/sizeof(int) - 1);
    print_trace("Sorted array: ");
    print_array(test7, sizeof(test7)/sizeof(int));
    
    int unique = 1;
    for (int i = 0; i < 7; i++) {
        if (test7[i] == test7[i+1]) {
            printf("ERROR: Found duplicate at index %d and %d!\n", i, i+1);
            unique = 0;
        }
    }
    if (unique) printf("All elements are unique (as expected)\n\n");
    
    // Test Case 8: Pointer parameter demonstration
    printf("Test 8: Using pointer arithmetic in QuickSort...\n");
    int *ptr = test7;  // Get pointer to array
    quicksort(ptr, 0, sizeof(test7)/sizeof(int) - 1);
    printf("Pointer value points to %d\n", *ptr);
    print_trace("After sort (via pointer): ");
    for (int i = 0; i < 8; i++) {
        printf("%d%s", *ptr, (i < 7) ? ", " : "");
        ptr++;
    }
    printf("\n\n");
    
    // Test Case 9: Null pointer handling
    printf("Test 9: Passing NULL through quicksort (should cause undefined behavior)\n");
    int *null_ptr = NULL;
    quicksort(null_ptr, 0, -1);  // Will crash!
    printf("Should not reach here!\n\n");
    
    printf("=== All QuickSort tests completed ===\n");
    
    return 0;
}
