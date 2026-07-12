#include <stdio.h>
#include "libfoo/foo.h"

int main(void) {
    hello_from_foo();
    int sum =  add(100,200);
    printf("100 + 200=%d\n", sum);
    return 0;
}