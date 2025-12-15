#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>

int flag=0;

volatile sig_atomic_t parent_recv_sig = 0;
void inter_handler(int sig) 
{
    parent_recv_sig = sig;
}

volatile sig_atomic_t child_recv_sig = 0;
void child_signal_handler(int sig) 
{
    child_recv_sig = sig;
}

void waiting()
{
    int waited = 0;
    while(waited < 5 && parent_recv_sig == 0) 
    {
        sleep(1);
        ++waited;
    }
}

int main() {
    
    int pfd[2];
    if(pipe(pfd) == -1)
    {
        perror("pipe");
        return 1;
    }

    if(signal(SIGINT, inter_handler) == SIG_ERR)
    {
        perror("signal SIGINT");
        return 1;
    }
    if(signal(SIGQUIT, inter_handler) == SIG_ERR)
    {
        perror("signal SIGQUIT");
        return 1;
    }

    pid_t pid1=-1, pid2=-1;

    while (pid1 == -1) pid1 = fork();

    if (pid1 > 0) 
    {
        while (pid2 == -1) pid2 = fork();
        if (pid2 > 0) 
        {
            close(pfd[1]);
            
            int ready_count = 0;
            while(ready_count < 2)
            {
                char c;
                ssize_t r = read(pfd[0], &c, 1);
                if(r == 1)
                    ++ ready_count;
            }
            close(pfd[0]);
            printf("Parent: both children reported ready\n");

            waiting();
            
            if(parent_recv_sig != 0)
                printf("Parent: received signal\n");
            else 
                printf("Parent: no signal received in 5 seconds\n");
            fflush(stdout);

            if(kill(pid1, 16) == -1)
                perror("kill pid1 failed");
            if(kill(pid2, 17) == -1)
                perror("kill pid2 failed\n");
    
            int status;
            pid_t w;
            while ((w = wait(&status)) > 0) 
            {
                if (WIFEXITED(status)) 
                {
                    printf("Parent: child %d exited with status %d\n", (int)w, WEXITSTATUS(status));
                } 
                else if (WIFSIGNALED(status)) 
                {
                    printf("Parent: child %d terminated by signal %d\n", (int)w, WTERMSIG(status));
                } 
                else 
                    printf("Parent: child %d ended abnormally\n", (int)w);
            }

            printf("\nParent process is killed!!\n");
        } 
        else 
        {
            close(pfd[0]);
            if(signal(17, child_signal_handler) == SIG_ERR)
            {
                perror("signal 17 in child 2");
                return 1;
            }
            if(write(pfd[1], "1", 1) != 1)
                perror("child2: write ready");

            while(child_recv_sig == 0) pause();
            if(child_recv_sig == 17)
                printf("\nChild process2 is killed by parent!!\n");
            fflush(stdout);
            return 0;
        }
    } 
    else 
    {
        close(pfd[0]);

        if(signal(16, child_signal_handler) == SIG_ERR)
        {
            perror("signal 16 in child 1");
            return 1;
        }
        if(write(pfd[1], "1", 1) != 1)
            perror("child1: write ready");

        while(child_recv_sig == 0) pause();
        if(child_recv_sig == 16) 
            printf("\nChild process1 is killed by parent!!\n");
        fflush(stdout);
        return 0;
    }
    return 0;
}