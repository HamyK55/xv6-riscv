#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// this user program invokes system call pstate
int main(int argc, char *argv[])
{
    if (argc != 3){
        printf("Usage: set(int pid, int priority)\n");
    }else
    {
        set(atoi(argv[1]), atoi(argv[2]));
    }
    
    
    exit(0);
}