// history.
#include "kernel/types.h"
#include "user/user.h"


int main(char *argv, int argc){
    // open the file

    // read while there are bits

    // close the file

    // or 

    // fork 
        // exec cat sh_history

    char *args[] = {"cat", "sh_history", 0};
    int pid = fork();
    if (pid == 0){
        if (exec("cat", args) < 0){
            printf("exec failed\n");
        } 
        exit(0);
    }
    // wait for child to finish
    wait(&pid);
    exit(0);
}