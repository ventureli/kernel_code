#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/string.h>

static DEFINE_PER_CPU(long, cpuvar) = 5;
static long __percpu *cpualloc;


static int __init my_init(void) {

    int cpu;
    for_each_possible_cpu(cpu) {
        per_cpu(cpuvar, cpu) = 10 + (int)cpu;
        pr_info("Init:Cpuvar on cpu %d=%ld\n", cpu, per_cpu(cpuvar,cpu)); //这里有get 就得有put
    }
    // put_cpu_var(cpuvar);

    cpualloc = alloc_percpu(long); //返回的是每个cpu变量的指针
     for_each_possible_cpu(cpu) {
        *per_cpu_ptr(cpualloc, cpu) = 6688 + (int)cpu;
         //这里打出来的地址是不一样的，因为每个cpu都用不同的基地址
         pr_info("Init:(%p)Cpualloc:%d cpualloc(%p - offset:%lx)=%ld", cpualloc, cpu, per_cpu_ptr(cpualloc, cpu), __per_cpu_offset[cpu],*per_cpu_ptr(cpualloc, cpu));
    }
    return 0;
}

static void __exit my_exit(void){
    // int cpu;
    // for_each_possible_cpu(cpu)
    // {
    //     pr_info("Cpuvar: Cpu %d=%ld\n",cpu ,per_cpu(cpuvar, cpu));
    //     pr_info("CpuAlloc: %d=%ld\n",cpu ,*per_cpu_ptr(cpualloc, cpu));
    // }

    free_percpu(cpualloc);
    pr_info("Module exited success");
}


MODULE_AUTHOR("liwenqinag");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Get per cpu variable");

module_init(my_init);
module_exit(my_exit);



