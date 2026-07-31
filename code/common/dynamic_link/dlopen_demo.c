#include <stdio.h>
#include <dlfcn.h>

int main(void)
{
    void *handle;
    void (*hello)(void);
    int (*my_add)(int, int);
    int result;

    /* 1. 运行时加载 libdemo.so */
    handle = dlopen("./libdemo.so", RTLD_NOW);
    if (handle == NULL) {
        printf("dlopen failed: %s\n", dlerror());
        return -1;
    }
    printf("[main] dlopen success, handle=%p\n", handle);

    /* 2. 查找 hello_from_so 符号 */
    hello = dlsym(handle, "hello_from_so");
    if (hello == NULL) {
        printf("dlsym 'hello_from_so' failed: %s\n", dlerror());
        dlclose(handle);
        return -1;
    }
    printf("[main] dlsym 'hello_from_so' -> %p\n", hello);

    /* 3. 查找 add 符号 */
    my_add = dlsym(handle, "add");
    if (my_add == NULL) {
        printf("dlsym 'add' failed: %s\n", dlerror());
        dlclose(handle);
        return -1;
    }
    printf("[main] dlsym 'add' -> %p\n", my_add);

    /* 4. 调用 .so 中的函数 */
    hello();
    result = my_add(100, 200);
    printf("[main] add(100, 200) = %d\n", result);

    /* 5. 关闭句柄 */
    dlclose(handle);
    printf("[main] dlclose done\n");

    return 0;
}
