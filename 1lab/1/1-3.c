#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
    pid_t pid;
    pid = fork();
    if(pid < 0){
        fprintf(stderr, "Fork Failed");
        return 1;
    }
    else if(pid == 0){
        printf("In child process PID: %d\n", getpid());
        if(!execve("./system_call", NULL, NULL)){
            fprintf(stderr, "Execve Failed");
            return 1;
        }
    }
    else {
        printf("child process PID: %d\n", pid);
        printf("parent process PID: = %d\n", getpid());    
        wait(NULL);
    }
    return 0;
}