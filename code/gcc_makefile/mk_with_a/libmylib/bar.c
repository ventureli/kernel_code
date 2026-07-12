#include <stdlib.h>
#include <stdio.h>
#include "mylib.h"

void bar(void) {
    printf("This is bar() from libmylib.a\n");
}

int add(int a, int b) {
    return a + b;
}

