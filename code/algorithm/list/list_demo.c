#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "list.h"

struct num{
    int i; 
    struct list_head list;  //虽然叫list_head 其实也是list_node
};

int main() {

    LIST_HEAD(test_list);//定义mylist的list链表
    int i;
    struct num *num;
    struct num *num1;
    srand((unsigned)time(NULL));
    //插入节点
    printf("\ninsert 50 elements to link list:");
    // while(1)
    {
        for (i = 1; i<500000; i++) {
            num = (struct num *)malloc(sizeof(struct num));
            num->i = rand()%99999;
            list_add_tail(&num->list, &test_list);
        }
        i = 0;
        //输出
        printf("all node value::\n");
        list_for_each_entry(num, &test_list, list) { 
            printf("%15d", num->i);
            if (0 == ((i+1) %10 )) { printf("\n"); }
            i++;
        }
        printf("\n\n");
        /*删除操作*/
        list_for_each_entry_safe(num, num1, &test_list, list){
            list_del(&num->list);
            free(num);
        }
        if(list_empty(&test_list)) {
            printf("WQTK release all list success\n\n");
        }
        // sleep(1);
    }
    
    return 0;

}
