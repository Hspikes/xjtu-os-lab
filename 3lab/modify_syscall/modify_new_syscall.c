#include<stdio.h>
#include<sys/time.h>
#include<unistd.h>

int main()
{
    long ret = syscall(96, 10, 20); 
    printf("%ld\n", ret);
    return 0;
}
