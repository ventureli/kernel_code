#include <stdio.h>
#include "libdemo.h"

void hello_from_so(void)
{
    printf("[libdemo.so] Hello from shared library!\n");
}

int add(int a, int b)
{
    printf("[libdemo.so] add(%d, %d) called\n", a, b);
    return a + b;
}
