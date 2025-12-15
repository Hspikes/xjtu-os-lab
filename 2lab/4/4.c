#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_SEQ 1000
#define MAX_FRAMES 100
#define MODE_FIFO 1
#define MODE_LRU 2

void print_frames(int frames[], int frame_count)
{
    printf("[");
    for (int i = 0; i < frame_count; ++i)
    {
        if (frames[i] == -1) printf(" -");
        else printf(" %d", frames[i]);
        if (i < frame_count - 1) printf(" |");
    }
    printf(" ]");
}

void simulate_fifo(int seq[], int seq_len, int frame_count)
{
    int frames[MAX_FRAMES];
    for (int i = 0; i < frame_count; ++i)
        frames[i] = -1;
    int fifo_ptr = 0;
    int page_faults = 0;

    printf("\n--- FIFO 模拟开始 (frames=%d) ---\n", frame_count);
    for (int step = 0; step < seq_len; ++step)
    {
        int page = seq[step];
        printf("Request %3d : page %d -> ", step + 1, page);
        int hit = 0;
        for (int i = 0; i < frame_count; ++i)
        {
            if (frames[i] == page)
            {
                hit = 1;
                break;
            }
        }
        if (hit)
        {
            printf("HIT\t");
            print_frames(frames, frame_count);
            printf("\n");
            continue;
        }

        page_faults++;

        int placed = 0;
        for (int i = 0; i < frame_count; ++i)
        {
            if (frames[i] == -1)
            {
                frames[i] = page;
                printf("FAULT (placed in empty slot %d)\t", i);
                print_frames(frames, frame_count);
                printf("\n");
                placed = 1;
                /* advance fifo_ptr to next position after this slot so replacements follow FIFO order */
                fifo_ptr = (i + 1) % frame_count;
                break;
            }
        }
        if (placed) continue;

        int replaced = frames[fifo_ptr];
        frames[fifo_ptr] = page;
        printf("FAULT (replace page %d at slot %d)\t", replaced, fifo_ptr);
        print_frames(frames, frame_count);
        printf("\n");
        fifo_ptr = (fifo_ptr + 1) % frame_count;
    }

    printf("--- FIFO 结束: 缺页 %d / %d ，缺页率 %.2f%% ---\n",
           page_faults, seq_len, 100.0 * page_faults / seq_len);
}

void simulate_lru(int seq[], int seq_len, int frame_count)
{
    int frames[MAX_FRAMES];
    int last_used[MAX_FRAMES]; 
    for (int i = 0; i < frame_count; ++i)
    {
        frames[i] = -1;
        last_used[i] = -1;
    }
    int page_faults = 0;
    int time_counter = 0;

    printf("\n--- LRU 模拟开始 (frames=%d) ---\n", frame_count);
    for (int step = 0; step < seq_len; ++step)
    {
        int page = seq[step];
        time_counter++;
        printf("Request %3d : page %d -> ", step + 1, page);

        int hit_index = -1;
        for (int i = 0; i < frame_count; ++i)
        {
            if (frames[i] == page)
            {
                hit_index = i;
                break;
            }
        }
        if (hit_index != -1)
        {
            last_used[hit_index] = time_counter;
            printf("HIT (frame %d)\t", hit_index);
            print_frames(frames, frame_count);
            printf("\n");
            continue;
        }

        page_faults++;

        int placed = 0;
        for (int i = 0; i < frame_count; ++i)
        {
            if (frames[i] == -1)
            {
                frames[i] = page;
                last_used[i] = time_counter;
                printf("FAULT (placed in empty slot %d)\t", i);
                print_frames(frames, frame_count);
                printf("\n");
                placed = 1;
                break;
            }
        }
        if (placed)
            continue;
        int lru_index = 0;
        for (int i = 1; i < frame_count; ++i)
            if (last_used[i] < last_used[lru_index])
                lru_index = i;
        int replaced = frames[lru_index];
        frames[lru_index] = page;
        last_used[lru_index] = time_counter;
        printf("FAULT (LRU replace page %d at slot %d)\t", replaced, lru_index);
        print_frames(frames, frame_count);
        printf("\n");
    }

    printf("--- LRU 结束: 缺页 %d / %d ，缺页率 %.2f%% ---\n",
           page_faults, seq_len, 100.0 * page_faults / seq_len);
}

int read_manual_sequence(int seq[], int max_len)
{
    int n;
    printf("输入页面请求数 (1-%d): ", max_len);
    if (scanf("%d", &n) != 1) return 0;
    if (n <= 0 || n > max_len) return 0;
    printf("请输入 %d 个页面号（用空格或回车分隔）：\n", n);
    for (int i = 0; i < n; ++i)
        if (scanf("%d", &seq[i]) != 1) return i;
    return n;
}

int gen_random_sequence(int seq[], int max_len)
{
    int n, max_page;
    printf("生成随机序列: 请求数 (1-%d): ", max_len);
    if (scanf("%d", &n) != 1)
        return 0;
    if (n <= 0 || n > max_len)
        return 0;
    printf("页面编号范围: 0 .. (max_page-1), 输入 max_page (>=2): ");
    if (scanf("%d", &max_page) != 1)
        return 0;
    if (max_page < 2)
        max_page = 2;
    srand((unsigned)time(NULL));
    for (int i = 0; i < n; ++i)
        seq[i] = rand() % max_page;
    return n;
}

int main()
{
    int algo_mode = MODE_FIFO;
    int frames, seq_len;
    int seq[MAX_SEQ];

    printf("页面置换模拟 (FIFO / LRU)\n");
    printf("选择算法: 1 - FIFO, 2 - LRU : ");
    int choice;
    if (scanf("%d", &choice) != 1)
        return 0;
    if (choice == 1)
        algo_mode = MODE_FIFO;
    else
        algo_mode = MODE_LRU;

    printf("请输入物理帧数量 (1-%d): ", MAX_FRAMES);
    if (scanf("%d", &frames) != 1)
        return 0;
    if (frames <= 0 || frames > MAX_FRAMES)
        frames = 3;

    printf("页面序列输入方式: 1 - 随机生成, 2 - 手工输入 : ");
    if (scanf("%d", &choice) != 1)
        return 0;
    if (choice == 1)
    {
        seq_len = gen_random_sequence(seq, MAX_SEQ);
        if (seq_len <= 0)
        {
            printf("随机序列生成失败。\n");
            return 0;
        }
        printf("随机生成的序列（长度=%d）：\n", seq_len);
        for (int i = 0; i < seq_len; ++i)
        {
            printf("%d", seq[i]);
            if (i < seq_len - 1)
                printf(" ");
        }
        printf("\n");
    }
    else
    {
        seq_len = read_manual_sequence(seq, MAX_SEQ);
        if (seq_len <= 0)
        {
            printf("手工输入失败。\n");
            return 0;
        }
    }

    if (algo_mode == MODE_FIFO)
        simulate_fifo(seq, seq_len, frames);
    else
        simulate_lru(seq, seq_len, frames);

    printf("\n示例测试数据（可直接用于手工输入模式）:\n");
    printf("示例1: frames=3, seq_len=12\n");
    printf("序列: 7 0 1 2 0 3 0 4 2 3 0 3\n");
    printf("示例2: frames=4, seq_len=20 (随机局部性示例)\n");
    printf("序列: 1 2 3 1 4 1 2 5 1 2 3 6 1 2 3 4 5 6 1 2\n");

    return 0;
}
