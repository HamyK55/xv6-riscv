#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/stat.h"
#include "user/user.h"


// this user program invokes system call pstate
int main(int argc, char *argv[])
{
    struct proc_info user_buf[NCPU];
    int num_proc;
    num_proc = psinfo(user_buf);

    // Print the process information
    printf("pid\tname\tstate\t\tparent\n");
    printf("___________________________________________\n");
    for (int i = 0; i < num_proc; i++) {
        printf("%d\t%s\t%s\t%s\n", user_buf[i].pid, user_buf[i].name, user_buf[i].state, user_buf[i].pName);
    }

    printf("Total processes: %d\n", num_proc);
    exit(0);
}