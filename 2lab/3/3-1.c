#include <stdio.h>
#include <stdlib.h>
#define PROCESS_NAME_LEN 32   /*进程名长度*/
#define MIN_SLICE 10          /*最小碎片的大小*/
#define DEFAULT_MEM_SIZE 1024 /*内存大小*/
#define DEFAULT_MEM_START 0   /*起始位置*/

/* 内存分配算法 */
#define MA_FF 1
#define MA_BF 2
#define MA_WF 3

int mem_size = DEFAULT_MEM_SIZE; /*内存大小*/
int ma_algorithm = MA_FF;        /*当前分配算法*/
static int pid = 0;
int flag = 0;

struct free_block_type
{
    int size;
    int start_addr;
    struct free_block_type *next;
};

struct free_block_type *free_block;
int free_block_len;

struct allocated_block
{
    int pid;
    int size;
    int start_addr;
    char process_name[PROCESS_NAME_LEN];
    struct allocated_block *next;
};
/*进程分配内存块链表的首指针*/
struct allocated_block *allocated_block_head = NULL;

/*初始化空闲块，默认为一块，可以指定大小及起始地址*/
struct free_block_type *init_free_block(int mem_size)
{
    struct free_block_type *fb;
    free_block_len = 1;
    fb = (struct free_block_type *)malloc(sizeof(struct free_block_type));
    if (fb == NULL)
    {
        printf("No mem\n");
        return NULL;
    }
    fb->size = mem_size;
    fb->start_addr = DEFAULT_MEM_START;
    fb->next = NULL;
    return fb;
}

int cmpFF(const void *a, const void *b)
{
    const struct free_block_type *pa = a;
    const struct free_block_type *pb = b;
    if (pa->start_addr < pb->start_addr) return -1;
    if (pa->start_addr > pb->start_addr) return 1;
    return 0;
}

int cmpBF(const void *a, const void *b)
{
    const struct free_block_type *pa = a;
    const struct free_block_type *pb = b;
    if (pa->size < pb->size) return -1;
    if (pa->size > pb->size) return 1;
    return 0;
}

int cmpWF(const void *a, const void *b)
{
    const struct free_block_type *pa = a;
    const struct free_block_type *pb = b;
    if (pa->size > pb->size) return -1;
    if (pa->size < pb->size) return 1;
    return 0;
}

void rearrange_by(int (*comparator)(const void*, const void*)) 
{
    int len = free_block_len;
    struct free_block_type *q = free_block;
    struct free_block_type *tmp = malloc(len * sizeof(*tmp));

    for (int i = 0; i < len; ++i) {
        tmp[i] = *q;                
        struct free_block_type *tofree = q;
        q = q->next;
        free(tofree);    
    }

    qsort(tmp, len, sizeof(*tmp), comparator);

    struct free_block_type *head = NULL, *prev = NULL;
    for (int i = 0; i < len; ++i) 
    {
        struct free_block_type *node = malloc(sizeof(*node));
        node->size = tmp[i].size;
        node->start_addr = tmp[i].start_addr;
        node->next = NULL;
        if (prev) prev->next = node; 
        else head = node;
        prev = node;
    }
    free(tmp);

    free_block = head;
    free_block_len = len;
}

void rearrange(int algorithm)
{
    switch (algorithm)
    {
    case MA_FF:
        rearrange_by(cmpFF);
        break;
    case MA_BF:
        rearrange_by(cmpBF);
        break;
    case MA_WF:
        rearrange_by(cmpWF);
        break;
    }
}

void delete_free_block(struct free_block_type *pre, struct free_block_type *fbt)
{
    -- free_block_len;
    if(pre) pre->next = fbt->next;
    else free_block = fbt->next;
}

void insert_free_block(struct free_block_type *pre, struct free_block_type *fbt, struct free_block_type *nxt)
{
    ++ free_block_len;
    fbt->next = nxt;
    if(pre) pre->next = fbt;
    else free_block = fbt;
}

/* 比较已分配块按起始地址排序（用于 qsort） */
int cmp_alloc_by_addr(const void *a, const void *b)
{
    const struct allocated_block *pa = *(const struct allocated_block **)a;
    const struct allocated_block *pb = *(const struct allocated_block **)b;
    if (pa->start_addr < pb->start_addr) return -1;
    if (pa->start_addr > pb->start_addr) return 1;
    return 0;
}

int compact_memory()
{
    int n = 0;
    struct allocated_block *p = allocated_block_head;
    while (p) { ++n; p = p->next; }

    if (n == 0) return 1;

    struct allocated_block **arr = malloc(n * sizeof(*arr));
    if (!arr) return -1;
    int idx = 0;
    p = allocated_block_head;
    while (p) { arr[idx++] = p; p = p->next; }
    qsort(arr, n, sizeof(*arr), cmp_alloc_by_addr);

    int next_start = DEFAULT_MEM_START;
    for (int i = 0; i < n; ++i) {
        arr[i]->start_addr = next_start;
        next_start += arr[i]->size;
    }
    free(arr);

    struct free_block_type *fb = free_block;
    while (fb) {
        struct free_block_type *t = fb;
        fb = fb->next;
        free(t);
    }
    free_block = NULL;
    free_block_len = 0;

    if (next_start < mem_size) {
        struct free_block_type *nb = malloc(sizeof(*nb));
        if (!nb) {
            free_block = NULL;
            free_block_len = 0;
            return -1;
        }
        nb->start_addr = next_start;
        nb->size = mem_size - next_start;
        nb->next = NULL;
        free_block = nb;
        free_block_len = 1;
    } else {
        free_block = NULL;
        free_block_len = 0;
    }
    return 1;
}

int allocate_mem(struct allocated_block *ab)
{
    struct free_block_type *fbt, *pre;
    int request_size = ab->size;

    fbt = free_block;
    pre = NULL;
    while(fbt)
    {
        int freesize = fbt->size;
        if(freesize >= request_size)
        {
            ab->start_addr = fbt->start_addr;
            if(freesize - request_size <= MIN_SLICE)
            {
                delete_free_block(pre, fbt);
                free(fbt);
            }
            else
            {
                fbt->size = freesize - request_size;
                fbt->start_addr = fbt->start_addr + request_size;
                if(ma_algorithm == MA_BF) rearrange_by(cmpBF);
                else if(ma_algorithm == MA_WF) rearrange_by(cmpWF);
            }
            return 1;
        }
        pre = fbt;
        fbt = fbt->next;
    }

    int total_free = 0;
    fbt = free_block;
    while (fbt) 
    { 
        total_free += fbt->size; 
        fbt = fbt->next; 
    }

    if (total_free >= request_size) 
    {
        if (compact_memory() != 1) return -1;
        allocate_mem(ab);
    }

    return -1;
}

int free_mem(struct allocated_block *ab)
{
    int algorithm = ma_algorithm;
    struct free_block_type *fbt, *pre, *work;
    fbt = (struct free_block_type *)malloc(sizeof(struct free_block_type));
    if (!fbt) return -1;
    // 进行可能的合并，基本策略如下
    // 1. 将新释放的结点插入到空闲分区队列末尾
    // 2. 对空闲链表按照地址有序排列
    // 3. 检查并合并相邻的空闲分区
    // 4. 将空闲链表重新按照当前算法排序
    // 请自行补充……
    fbt->size = ab->size;
    fbt->start_addr = ab->start_addr;
    insert_free_block(NULL, fbt, free_block);
    rearrange_by(cmpFF);
    fbt = free_block;
    pre = NULL;
    while(fbt)
    {
        if(pre) 
        {
            if(pre->start_addr + pre->size == fbt->start_addr )
            {
                pre->size += fbt->size;
                delete_free_block(pre, fbt);
                free(fbt);
                fbt = pre;
            }
        }
        pre = fbt;
        fbt = fbt->next;
    }
    if(algorithm == 2) rearrange_by(cmpBF);
    else if(algorithm == 3) rearrange_by(cmpWF);
    return 1;
}

void display_menu()
{
    printf("\n");
    printf("1 - Set memory size (default=%d)\n", DEFAULT_MEM_SIZE);
    printf("2 - Select memory allocation algorithm\n");
    printf("3 - New process \n");
    printf("4 - Terminate a process \n");
    printf("5 - Display memory usage \n");
    printf("0 - Exit\n");
}

int set_mem_size()
{
    int size;
    if (flag != 0)
    { 
        printf("Cannot set memory size again\n");
        return 0;
    }
    printf("Total memory size =");
    scanf("%d", &size);
    if (size > 0)
    {
        mem_size = size;
        free_block->size = mem_size;
    }
    flag = 1;
    return 1;
}

void set_algorithm()
{
    int algorithm;
    printf("\t1 - First Fit\n");
    printf("\t2 - Best Fit \n");
    printf("\t3 - Worst Fit \n");
    scanf("%d", &algorithm);
    if (algorithm >= 1 && algorithm <= 3)
        ma_algorithm = algorithm;
    // 按指定算法重新排列空闲区链表
    rearrange(ma_algorithm);
}

/*创建新的进程，主要是获取内存的申请数量*/
int new_process()
{
    struct allocated_block *ab;
    int size;
    int ret;
    ab = (struct allocated_block *)malloc(sizeof(struct allocated_block));
    if(!ab)
        exit(-5);
    ab->next = NULL;
    pid++;
    sprintf(ab->process_name, "PROCESS-%02d", pid);
    ab->pid = pid;
    printf("Memory for %s:", ab->process_name);
    scanf("%d", &size);
    if (size > 0)
        ab->size = size;
    ret = allocate_mem(ab); /* 从空闲区分配内存，ret==1表示分配ok*/
    /*如果此时allocated_block_head尚未赋值，则赋值*/
    if ((ret == 1) && (allocated_block_head == NULL))
    {
        allocated_block_head = ab;
        return 1;
    }
    /*分配成功，将该已分配块的描述插入已分配链表*/
    else if (ret == 1)
    {
        ab->next = allocated_block_head;
        allocated_block_head = ab;
        return 2;
    }
    else if (ret == -1)
    { /*分配不成功*/
        printf("Allocation fail\n");
        free(ab);
        return -1;
    }
    return 3;
}

/*释放ab数据结构节点*/
int dispose(struct allocated_block *free_ab)
{
    struct allocated_block *pre, *ab;
    if (free_ab == allocated_block_head)
    { /*如果要释放第一个节点*/
        allocated_block_head = allocated_block_head->next;
        free(free_ab);
        return 1;
    }
    pre = allocated_block_head;
    ab = allocated_block_head->next;
    while (ab != free_ab)
    {
        pre = ab;
        ab = ab->next;
    }
    pre->next = ab->next;
    free(ab);
    return 2;
}

/*删除进程，归还分配的存储空间，并删除描述该进程内存分配的节点*/
struct allocated_block * find_process(int pid)
{
    struct allocated_block *p = allocated_block_head;
    while(p)
    {
        if(p->pid == pid) return p;
        else p = p->next;
    }
    return NULL;
}

void kill_process()
{
    struct allocated_block *ab;
    int pid;
    printf("Kill Process, pid=");
    scanf("%d", &pid);
    ab = find_process(pid);
    if (ab != NULL)
    {
        free_mem(ab); /*释放ab所表示的分配区*/
        dispose(ab);  /*释放ab数据结构节点*/
    }
}

/* 显示当前内存的使用情况，包括空闲区的情况和已经分配的情况 */
int display_mem_usage()
{
    struct free_block_type *fbt = free_block;
    struct allocated_block *ab = allocated_block_head;
    if (fbt == NULL)
        return (-1);
    printf("----------------------------------------------------------\n");
    /* 显示空闲区 */
    printf("Free Memory:\n");
    printf("%20s %20s\n", " start_addr", " size");
    while (fbt != NULL)
    {
        printf("%20d %20d\n", fbt->start_addr, fbt->size);
        fbt = fbt->next;
    }
    printf("free_block_len = %d\n", free_block_len);
    /* 显示已分配区 */
    printf("\nUsed Memory:\n");
    printf("%10s %20s %10s %10s\n", "PID", "ProcessName", "start_addr", " size");
    while (ab != NULL)
    {
        printf("%10d %20s %10d %10d\n", ab->pid, ab->process_name, ab->start_addr, ab->size);
        ab = ab->next;
    }
    printf("----------------------------------------------------------\n");
    return 0;
}

void do_exit()
{
    struct allocated_block *p = allocated_block_head;
    while(p)
    {
        struct allocated_block *tmp = p;
        p = p->next;
        free(tmp);
    }

    struct free_block_type *fbt = free_block;
    while(fbt)
    {
        struct free_block_type *tmp = fbt;
        fbt = fbt->next;
        free(tmp);
    }
}

int main()
{
    char choice;
    pid = 0;
    free_block = init_free_block(mem_size); // 初始化空闲区
    while (1)
    {
        display_menu(); // 显示菜单
        fflush(stdin);
        choice = getchar(); // 获取用户输入
        switch (choice)
        {
        case '1':
            set_mem_size();
            break; // 设置内存大小
        case '2':
            set_algorithm();
            flag = 1;
            break; // 设置算法
        case '3':
            new_process();
            // display_mem_usage();
            flag = 1;
            break; // 创建新进程
        case '4':
            kill_process();
            // display_mem_usage();
            flag = 1;
            break; // 删除进程
        case '5':
            display_mem_usage();
            flag = 1;
            break; // 显示内存使用
        case '0':
            do_exit();
            return 0; // 释放链表并退出
        default:
            break;
        }
    }
    return 0;
}