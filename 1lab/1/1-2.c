#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

int value = -1;

int main()
{
    pid_t pid, pid1;
    pid = fork();
    if(pid < 0){
        fprintf(stderr, "Fork Failed");
        return 1;
    }
    else if(pid == 0){
        pid1 = getpid();
        value ++;
        printf("child: value = %d\n", value);
        printf("child: *value = %p\n", &value);
    }
    else {
        pid1 = getpid();
        value --;
        printf("parent: value = %d\n", value);
        printf("parent: *value = %p\n", &value);    
        wait(NULL);
    }
    value += 5;
    if(pid) printf("parent: before return value = %d\n", value);
    else printf("child: before return value = %d\n", value); 
    return 0;
}