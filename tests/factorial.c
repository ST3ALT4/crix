#include <stdio.h>

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    int sub = factorial(n - 1);
    int res = n * sub;
    return res;
}

int main(void) {
    int n = 5;
    int res = factorial(n);
    printf("factorial(%d) = %d\n", n, res);
    return 0;
}
