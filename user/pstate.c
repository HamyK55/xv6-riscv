#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[])
{
    printf("I will call system call pstate in 1 second...\n");
    sleep(1);
    pstate();
   
    exit(0);
}