#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define N 100000

long long counter = 0;

void *worker() {
    for (int i = 0; i < N; ++i) 
        counter += 1;
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    if (pthread_create(&t1, NULL, worker, NULL) != 0) {
        perror("pthread_create t1");
        return 1;
    }
    if (pthread_create(&t2, NULL, worker, NULL) != 0) {
        perror("pthread_create t2");
        return 1;
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("expected = %lld, actual counter = %lld\n", 2 * N, counter);
    return 0;
}
