#include <stdlib.h>

typedef enum { RED, GREEN, BLUE } Color;

typedef struct {
    int   x;
    int   y;
    Color color;
} Point;

static int depth = 0;

int recursive(int n)
{
    depth++;
    int local = n - 1;
    if (local > 0) {
        int deeper = recursive(local);
        return deeper + 1;
    }
    return 0;
}

Point make_point(int x, int y, Color c)
{
    Point p;
    p.x     = x;
    p.y     = y;
    p.color = c;
    return p;
}

int main(void)
{
    Point pts[3];
    for (int i = 0; i < 3; i++) {
        int val = i * 10;
        pts[i]  = make_point(val, val + 1, (Color)i);
    }
    int total = recursive(5);
    (void)total;
    return 0;
}
