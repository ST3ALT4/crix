#include <stdio.h>

int global_counter = 0;

static float compute(int x, float y)
{
    int   local_a = x * 2;
    float local_b = y + 1.0f;
    return local_a + local_b;
}

int main(void)
{
    int   result;
    char  msg[64];

    result = (int)compute(3, 2.5f);
    global_counter++;

    if (result > 5) {
        int inside_if = result - 5;
        snprintf(msg, sizeof msg, "result=%d inside_if=%d", result, inside_if);
        puts(msg);
    }

    return 0;
}
