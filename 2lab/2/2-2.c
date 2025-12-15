#include <unistd.h>
#include <signal.h>
#include <stdio.h>

int pid1 = -1, pid2 = -1; // 定义两个进程变量

int main( ) {
    int fd[2];
    char InPipe[1000]; // 定义读缓冲区
    char c1='1', c2='2';
    pipe(fd); // 创建管道
    if((pid1 = fork( )) < 0)
    {
        perror("Fork 1");
        return 0;
    }
    if(pid1 !=0)
        if((pid2 = fork( )) < 0)
        {
            perror("Fork 2");
            return 0;
        }
    if(pid1 == 0) {
        close(fd[0]);
        // lockf(fd[1],1,0);
        // sleep(1);
        for(int i = 0; i < 100; ++i)
            write(fd[1], &c1, 1);
        // lockf(fd[1],0,0);
        return 0;
    }
    else if(pid2 == 0){
        close(fd[0]);
        // lockf(fd[1],1,0);
        // sleep(1);
        for(int i = 0; i < 100; ++i)
            write(fd[1], &c2, 1);
        // lockf(fd[1],0,0);
        return 0;
    }
    else {
        close(fd[1]);
        waitpid(pid1, NULL, 0);
        wait(0); 
        ssize_t total = 0;
        while(total < 200)
        {
            ssize_t r = read(fd[0], &InPipe[total], 200 - total);
            if(r > 0)
                total += r;
            else if(r == 0)
                break;
            else
            {
                perror("Read");
                break;
            }
        } 
        if (total <= 200) InPipe[total] = '\0';
        else InPipe[200] = '\0';
        int count1,count2;
        count1 = count2 = 0;
        for(int i = 0;i < total; ++i)
        {
            if(InPipe[i] == '1') count1++;
            else if(InPipe[i] == '2') count2++;
        }
        printf("total = %d count1 = %d count2 = %d\n%s\n",total, count1, count2, InPipe); // 显示读出的数据
        return 0;
    }   
}