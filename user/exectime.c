#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char *argv[])
{
    // print uptime
    printf("first Current system time: %d\n", uptime());
    // fork
    int pid = fork();
    
    if (pid == 0){
        // execute the command in the args list (second arg ...)
        argv++;
        if (exec(argv[0], argv) < 0){
            printf("exec failed\n");
        }
        exit(0);
    }

    // wait for child to finish
    wait(&pid);
    
    // print uptime
    printf("last Current system time: %d\n", uptime());

   
    exit(0);
}