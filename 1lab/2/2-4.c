// thread_execve.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/syscall.h> // for SYS_gettid
#include <errno.h>
#include <string.h>

extern char **environ;

#define TARGET_PATH "./system_call" 

void *worker(void *arg) {
    int id = *(int *)arg;
    free(arg);
    printf("thread%d create success!\n",id);

    pid_t pid = getpid();
    long tid = syscall(SYS_gettid);
    
    printf("thread%d: PID=%d, TID=%ld\n\n", id, (int)pid, tid);
    fflush(stdout);
    
    if (execve(TARGET_PATH, NULL, NULL) == -1) {
        fprintf(stderr, "thread%d execve failed: %s\n", id, strerror(errno));
        _exit(EXIT_FAILURE);
    }

    return NULL;
}

int main(void) {
    pthread_t t1, t2;

    int *id1 = malloc(sizeof(int));
    int *id2 = malloc(sizeof(int));
    if (!id1 || !id2) {
        perror("malloc");
        return 1;
    }
    *id1 = 1;
    *id2 = 2;

    if (pthread_create(&t1, NULL, worker, id1) != 0) {
        perror("pthread_create t1");
        free(id1);
        free(id2);
        return 1;
    }
    if (pthread_create(&t2, NULL, worker, id2) != 0) {
        perror("pthread_create t2");
        pthread_join(t1, NULL);
        free(id1);
        free(id2);
        return 1;
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    free(id1);
    free(id2);

    return 0;
}
