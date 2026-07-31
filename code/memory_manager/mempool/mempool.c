#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define ALLOC_SIZE 8
#define LOOP 5
#define MAX_POOL_SIZE  1024 * 1024
#define BLOCK_SIZE 64

//内存映射表,对应物理内存？
typedef struct memory_map_table {
    char *p_block;
    int index;
    int used;
}Memory_Map_Table;

//内存分配表
typedef struct memory_alloc_table {
    char *p_start;
    int used;
    int block_start_index;
    int block_cnt;
}Memory_Alloc_Table;

typedef struct memory_pool{
    char * memory_start;  //这个起始的指针？
    Memory_Alloc_Table *alloc_table;
    Memory_Map_Table *map_table;
    int total_size;
    int internal_total_size;
    int increment;  ///增量
    int used_size;   //
    int block_size; //块大小
    int block_cnt;
    int alloc_cnt;  //分配多少
}Memory_Pool;


Memory_Map_Table *map_table_pos(const Memory_Pool *pool) {
    Memory_Map_Table *pm = (Memory_Map_Table *)(pool->memory_start +  sizeof(Memory_Pool));
    return pm;
}
Memory_Alloc_Table *alloc_table_pos(const Memory_Pool *pool) {
    Memory_Alloc_Table *pm = (Memory_Alloc_Table *)(pool->memory_start +  sizeof(Memory_Pool) +  sizeof(Memory_Map_Table) * (pool->block_cnt));
    return pm;
}

void dumpPoolInfo(const Memory_Pool * const pool) ;
char * memory_pos(const Memory_Pool *pool) {
    //
    char *pm = (char *)(pool->memory_start + sizeof(Memory_Pool) + (sizeof(Memory_Map_Table) +  sizeof(Memory_Alloc_Table)) *pool->block_cnt);
    return pm;
}

Memory_Pool *memory_pool_init(int size, int increment) {
    char *p = NULL;
    char *p_memory  = NULL;
    Memory_Pool *pool = NULL;
    Memory_Alloc_Table *alloc_table = NULL;
    Memory_Alloc_Table *p_alloc_table = NULL;
    Memory_Map_Table *map_table = NULL;
    Memory_Map_Table *p_map_table = NULL;
    int block_cnt = 0;
    int all_size = 0 ;
    int i = 0 ;
    if (size < 0 || size > MAX_POOL_SIZE) {
        printf("memory_pool_init invaid size:%d", size);
        return pool;
    }
    block_cnt =  (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    all_size =  sizeof(Memory_Pool) + (sizeof(Memory_Map_Table) + sizeof(Memory_Alloc_Table)) * block_cnt + block_cnt * BLOCK_SIZE;

    p = (char *)malloc(all_size);
    if (p == NULL) {
        perror("Malloc Failed");
        return pool;
    }   


    memset(p, 0, all_size);
    pool = (Memory_Pool *)p;
    pool->block_cnt = block_cnt;
    pool->block_size = BLOCK_SIZE;
    pool->increment = increment;
    pool->internal_total_size = all_size;
    pool->total_size =  size;
    pool->used_size =  0;
    pool->alloc_cnt = 0 ;
    pool->memory_start = p;
    p_memory = memory_pos(pool);
    map_table = map_table_pos(pool);
    alloc_table = alloc_table_pos(pool);

    //初始化物理块（伪）的映射表
     for (int i = 0 ; i < block_cnt; i++) {
        p_map_table = (Memory_Map_Table *)((char *)map_table + i * sizeof(Memory_Map_Table));
        p_map_table->index = i;
        p_map_table->p_block = (char *)((char *)p_memory + BLOCK_SIZE * i); //初始化物理地起始指针
        p_map_table->used = 0; 
    }
    
    //初始化分配表
    for (int i = 0 ; i < block_cnt; i++) {
        p_alloc_table = (Memory_Alloc_Table *)((char *)alloc_table + i * sizeof(Memory_Alloc_Table));
        p_alloc_table->block_cnt = 0;
        p_alloc_table->block_start_index = -1;
        p_alloc_table->p_start = NULL;
        p_alloc_table->used = 0;
    }
    dumpPoolInfo(pool);
    return pool;
   
}

void dumpPoolInfo(const Memory_Pool * const pool) {
    if (pool == NULL) {
        printf("dumpPoolInfo null pool");
        return;
    }
    Memory_Alloc_Table *alloc_table = alloc_table_pos(pool);
    Memory_Alloc_Table *p_alloc_table = NULL;
    Memory_Map_Table *map_table = map_table_pos(pool);
    Memory_Map_Table *p_map_table = NULL;
    char *block_table = memory_pos(pool);
    char *p_block_table = NULL;

    int block_cnt = pool->block_cnt;;
    int all_size = pool -> internal_total_size ;
    printf("-------------------------内存信息如下-------------------------\n");
    
    printf("\nmemory_pool_init:total   size:%d, all_size:%d block cnt:%d,block size:%d used:%d \n\n",pool->total_size, all_size, pool->block_cnt, BLOCK_SIZE, pool->used_size);
    printf("-------------------------内存布局如下-------------------------\n");
    printf("|-------------%p-----------------|\n",(void *)(pool->memory_start));
    printf("|-------------Memory_Pool object(%ld)-------|\n",sizeof(Memory_Pool));
    printf("|-------------%p-----------------|\n",(void *)map_table);
    for (int i = 0 ; i < block_cnt; i++) {
        p_map_table = (Memory_Map_Table *)((char *)map_table + i * sizeof(Memory_Map_Table));
        printf("|------(%p) Memory_Map_Table(%ld)-------------|\n",(void *)p_map_table,sizeof(Memory_Map_Table));
        printf("|                    (%d,%d,%p)                 |\n",p_map_table->index,p_map_table->used, (void *) p_map_table->p_block );
    }

    printf("|-------------%p-----------------|\n",(void *)alloc_table);
    for (int i = 0 ; i < block_cnt; i++) {
         p_alloc_table = (Memory_Alloc_Table *)((char *)alloc_table + i * sizeof(Memory_Alloc_Table));
        printf("|------(%p) Memory_Alloc_Table(%ld)-------------|\n",(void *)p_alloc_table,sizeof(Memory_Alloc_Table));
        printf("|                    (usd%d,cnt:%d,start:%d,%p)                 |\n",p_alloc_table->used,p_alloc_table->block_cnt,p_alloc_table->block_start_index, (void *) p_alloc_table->p_start );
    }

    printf("|-------------%p-----------------|\n",(void *)block_table);
    for (int i = 0 ; i < block_cnt; i++) {
        p_block_table = ((char *)block_table + i * (BLOCK_SIZE));
        printf("|------(%p) block (%d)-------------|\n",(void *)p_block_table,BLOCK_SIZE);
    }

}

void * Memory_malloc(Memory_Pool *pool, int size)  {
    char *p_start = NULL;
    int need_block_cnt = 0;
    Memory_Alloc_Table *alloc_table = NULL;
    Memory_Alloc_Table *p_alloc_table = NULL;
    Memory_Map_Table *map_table = NULL;
    Memory_Map_Table *p_map_table = NULL;

    int block_cnt = 0;
    int start_index =  -1;
    int i = 0; 
    if(size <= 0) {
        printf("\nInvalid size(%d)\n",size);
        return p_start;
    }

    if (size > pool->total_size) {
        printf("\n %d is more than total size;\n",size);
        return p_start;
    }
    if (size > (pool->total_size - pool->used_size)) {
        printf("\n Free memory (%d) is more than allocated(%d)\n", (pool->total_size - pool->used_size), size);
        return p_start;
    }
    need_block_cnt = (size + BLOCK_SIZE -1) / BLOCK_SIZE;
    map_table = map_table_pos(pool);

    start_index = -1;
    printf("want size:%d  need blk %d \n",size, need_block_cnt);

    
    for(int i = 0 ; i < pool->block_cnt; i++) {
        p_map_table = (Memory_Map_Table *)((char *)map_table + i*sizeof(Memory_Map_Table));
        if (p_map_table -> used)  {  //放弃这段，我们需要一个连续的地址的页面
            block_cnt = 0;
            start_index = -1;
            continue;
        }
        if(start_index == -1)
        {
            start_index = i;
        }
        block_cnt ++;
        if(block_cnt ==  need_block_cnt) {
            break;
        }
    }
    if (start_index == -1) {
        printf("No enough space availabe memory to used");
        return NULL;
    }
    if (block_cnt < need_block_cnt) {
        printf("not enough continual space ");
        return NULL;
    }
    
    

     printf("start index %d\n",start_index);
    alloc_table = alloc_table_pos(pool);
    for (int i = 0 ; i < pool->block_cnt; i++) {
        p_alloc_table = (Memory_Alloc_Table *)((char *)alloc_table + i * sizeof(Memory_Alloc_Table));
        if(0 == p_alloc_table) {
            p_alloc_table = NULL;
            break;
        }
        if (p_alloc_table -> used == 1) { //找到一个分配表，记录下来
            continue;
        }else{
            break;
        }  
    }

    if (p_alloc_table == NULL) {
         printf("alloc table === null");
        return NULL;
    }

    map_table = map_table_pos(pool);
    p_map_table = (Memory_Map_Table *)((char *)map_table + start_index * sizeof(Memory_Map_Table));
    p_alloc_table->p_start = p_map_table->p_block;
    p_alloc_table->block_start_index = p_map_table->index;
    p_alloc_table->block_cnt = block_cnt;
    p_alloc_table->used =  1;


    for (int i = start_index ; i < start_index + need_block_cnt; i++) {
       p_map_table = (Memory_Map_Table *)((char *)map_table + i * sizeof(Memory_Map_Table));
       p_map_table ->used = 1;
    }
    printf("\nAlloc size:%d,Block:(start :%d ,end %d,cnt:%d)\n",size , start_index ,start_index
     + block_cnt -1 ,block_cnt);
    pool->alloc_cnt ++;
    pool->used_size += need_block_cnt * BLOCK_SIZE ; //size ?
    return p_alloc_table->p_start;

}

int memory_free(Memory_Pool * pool, void *memory) {
    Memory_Alloc_Table *alloc_table = NULL;
    Memory_Alloc_Table *p_alloc_table = NULL;
    Memory_Map_Table *map_table = NULL;
    Memory_Map_Table *p_map_table = NULL;
    int i  = 0;
    int block_start_index = 0 ;
    int block_cnt = 0;
    if (memory == NULL) {
        printf("memory_free() memory is null");
        return 0;
    }
    if (pool == NULL) {
        printf("pool is NULL");
        return 0;
    }
    alloc_table =  alloc_table_pos(pool);
    for(int i = 0 ; i < pool->block_cnt ;i++)
    {
        p_alloc_table  =  (Memory_Alloc_Table *)((char *)alloc_table + i*sizeof(Memory_Alloc_Table));
        if (p_alloc_table->p_start == memory) {
            block_start_index = p_alloc_table->block_start_index;
            block_cnt =  p_alloc_table->block_cnt;
            break;
        }
    }
    if(block_cnt == 0)
    {
        return 0;
    }
    map_table=  map_table_pos(pool);
    printf("Block:Free:%p :Start:%d,end:%d,cnt:%d\n",(void *)memory, block_start_index, block_start_index + block_cnt, block_cnt);
    for (i = block_start_index ;i < block_start_index + block_cnt; i++ ) {
        p_map_table = (Memory_Map_Table *)((char *)map_table + i * sizeof(Memory_Map_Table));
        p_map_table->used = 0;  //模拟了释放
    }
    p_alloc_table->used = 0;    //这里直接释放调了
    p_alloc_table->block_start_index = -1;    //这里直接释放调了
    p_alloc_table->block_cnt = 0;  
    pool->used_size -= block_cnt * BLOCK_SIZE;

    return 0;
}
void memory_pool_destroy(Memory_Pool *pool) {
    if (pool == NULL) {
        printf("Memory_pool_destroy:pool is NULL \n");
        return;
    }
    free(pool);
    pool = NULL;
}

int main() {

    Memory_Pool *pool = NULL;
    char *p1 = NULL;
    char *p2 = NULL;
    int i = 0 ;
    pool = memory_pool_init(256, 64);
    if(pool == NULL)
    {
        printf("内存池初始化失败");
    }
    int want_memory = 100;
    p1 = (char *)Memory_malloc(pool, want_memory); //占用两个block
    if (p1 == NULL) {
        printf("malloc failed.\n");
    }else{
        printf("分配成功:起始地址%p,大小：%d\n",(void *)p1 , want_memory);
        dumpPoolInfo(pool);
    }
    
    want_memory =  20;  //1个字节
    p2 = (char *)Memory_malloc(pool, want_memory); //占用两个block
    if (p2 == NULL) {
        printf("malloc failed.\n");
    }else{
        printf("分配成功:起始地址%p,大小：%d\n",(void *)p2 , want_memory);
        dumpPoolInfo(pool);
    }
    
    // for(int i = 0; i < 2; i++) {
    //     p1 = (char *)Memory_malloc(pool, ALLOC_SIZE);
    //     if (p1 == NULL) {
    //         printf("malloc failed.\n");
    //     }else{
    //         printf("分配成功");
    //     }
    memory_free(pool,p1);
    printf("释放p1后\n");
    dumpPoolInfo(pool);

    //重新分配失败
    want_memory = 192; //需要三个
    p1 = (char *)Memory_malloc(pool, want_memory); //占用两个block
    if (p1 == NULL) {
        printf("malloc failed.\n");
    }else{
        printf("分配成功:起始地址%p,大小：%d\n",(void *)p1 , want_memory);
        dumpPoolInfo(pool);
    }

    memory_free(pool,p2);
    printf("释放p2后\n");
    dumpPoolInfo(pool);
    // }

    return 0;
}