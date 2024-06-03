// history.
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"


int main(char *argv, int argc){

    //****************************Using System calls to read file

    int history_fd;
    char buf[100];
    // open sh_history
    if ((history_fd = open("sh_history", O_CREATE|O_RDWR)) == -1){
    fprintf(2, "error opening sh_history");
    }
    // read while there are bits, read
    while (read(history_fd, buf, sizeof(buf)))
    {
        printf(buf);
    }
    close(history_fd);
    
    //****************************Using exec cat to read file

    /*
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
    */
    exit(0);
}