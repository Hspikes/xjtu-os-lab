#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <unistd.h>

#define N 100

long long counter = 0;
sem_t sem;

void *worker() {
    for (int i = 0; i < N; ++i) {
        // printf("running\n");
        if(sem_wait(&sem) != 0) {
            perror("sem_wait");
            pthread_exit((void*)1);
        }
        counter += 1;
        if(sem_post(&sem) != 0) {
            perror("sem_post");
            pthread_exit((void*)1);
        }
    }
}

int main(void) {
    pthread_t t1, t2;
    sem_init(&sem, 0, 1);
    if (pthread_create(&t1, NULL, worker, NULL) != 0) {
        perror("pthread_create t1");
        sem_destroy(&sem);
        return 1;
    }
    if (pthread_create(&t2, NULL, worker, NULL) != 0) {
        perror("pthread_create t2");
        sem_destroy(&sem);
        return 1;
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("expected = %lld, actual counter = %lld\n", 2 * N, counter);

    sem_destroy(&sem);
    return 0;
}
